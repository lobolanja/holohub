#include <connext_lib/resource/resource_managers_dds.hpp>
#include "connext_lib/config/config.hpp"
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "dds/core/QosProvider.hpp"
#include "dds/sub/find.hpp"
#include "ndds/dds_c/dds_c_builtin.h"
#include "ndds/hpp/rti/topic/BuiltinTopicImpl.hpp"
#include "rti/core/Guid.hpp"
#include "rti/core/policy/CorePolicy.hpp"
#include <holoscan/logger/logger.hpp>

namespace {

constexpr char kBufferIdProperty[] = "connext_lib.receiver.buffer_id";
constexpr char kChannelProperty[] = "connext_lib.receiver.channel";
constexpr char kGuidProperty[] = "connext_lib.receiver.reader_guid";
constexpr char kGpuDirectEnabledProperty[] = "connext_lib.receiver.gpu_direct.enabled";
constexpr char kGpuDirectFastDestIpProperty[] = "connext_lib.receiver.gpu_direct.fast_dest_ip";
constexpr char kGpuDirectFastDestMacProperty[] = "connext_lib.receiver.gpu_direct.fast_dest_mac";
constexpr char kGpuDirectFastDestPortProperty[] = "connext_lib.receiver.gpu_direct.fast_dest_port";

std::string GuidToString(const rti::core::Guid& guid) {
  std::ostringstream oss;
  oss << guid;
  return oss.str();
}

}  // namespace

namespace connext_lib {

DdsReceiverResourcesManager::DdsReceiverResourcesManager(
    dds::domain::DomainParticipant participant,
    const connext_lib::AnoConfig& config)
    : participant_(std::move(participant)),
      subscriber_(participant_),
      topic_(participant_, config.channel_name(), dds::topic::qos::TopicQos()),
      // TODO: If the properties delays in propagation, consider enabling the datareader after the
      //properties are aplied in the qos's
      reader_(subscriber_, topic_,
              dds::core::QosProvider::Default().datareader_qos(
                  "BuiltinQosLib::Generic.KeepLastReliable.Transient")),
      buffer_id_(config.buffer_id()),
      channel_(topic_.name()),
      ano_config_(config) {}

rti::core::policy::Property DdsReceiverResourcesManager::buildProperties()
    const {
  rti::core::policy::Property properties;
  properties.set({kBufferIdProperty, buffer_id_}, true);
  properties.set({kChannelProperty, channel_}, true);
  properties.set({kGuidProperty, guidString()}, true);
  // Add GPUDirectReceiver properties when configured
  const auto& gpu_cfg = ano_config_.ano_network_config();
  if (gpu_cfg.enabled()) {
    properties.set({kGpuDirectEnabledProperty, std::string("true")}, true);
  }
  // Always publish fast destination fields (use defaults from config when not explicitly enabled).
  properties.set({kGpuDirectFastDestIpProperty, gpu_cfg.fast_ip()}, true);
  properties.set({kGpuDirectFastDestMacProperty, gpu_cfg.fast_mac_address()}, true);
  properties.set({kGpuDirectFastDestPortProperty, std::to_string(gpu_cfg.fast_port())}, true);
  return properties;
}

bool DdsReceiverResourcesManager::announce() {
  return applyProperties(buildProperties());
}

bool DdsReceiverResourcesManager::applyProperties(
    const ReceiverPropertySet& properties) {
  auto qos = reader_.qos();
  auto property_policy = qos.policy<rti::core::policy::Property>();
  const auto entries = properties.get_all();
  for (const auto& entry : entries) {
    property_policy.set(entry, properties.propagate(entry.first));
  }
  qos.policy(property_policy);
  reader_.qos(qos);
  return true;
}

std::string DdsReceiverResourcesManager::guidString() const {
  auto protocol =
      reader_.qos().policy<rti::core::policy::DataReaderProtocol>();
  return GuidToString(protocol.virtual_guid());
}

DdsSenderResourcesManager::DdsSenderResourcesManager(
    dds::domain::DomainParticipant participant,
    std::string channel)
    : participant_(std::move(participant)),
      publisher_(participant_),
      topic_(participant_, std::move(channel), dds::topic::qos::TopicQos()),
      writer_(publisher_,
              topic_,
              dds::core::QosProvider::Default().datawriter_qos(
                  "BuiltinQosLib::Generic.KeepLastReliable.Transient")),
      subscription_reader_(dds::core::null),
      channel_filter_(topic_.name()) {
  dds::sub::Subscriber builtin = dds::sub::builtin_subscriber(participant_);
  std::vector<
      dds::sub::DataReader<dds::topic::SubscriptionBuiltinTopicData>>
      readers;
  dds::sub::find<dds::sub::DataReader<dds::topic::SubscriptionBuiltinTopicData>>(
      builtin, DDS_SUBSCRIPTION_TOPIC_NAME, std::back_inserter(readers));
  if (readers.empty()) {
    throw std::runtime_error(
        "Failed to locate subscription builtin topic reader");
  }
  subscription_reader_ = readers.front();
}

void DdsSenderResourcesManager::pollOnce() {

  //TODO: Delete this
  //dds::core::BytesTopicType sample;
  //writer_.write(sample);
  //writer_.wait_for_acknowledgments(dds::core::Duration(10));
  auto samples = subscription_reader_.take();
  for (const auto& sample : samples) {
    if (!sample.info().valid()) {
      continue;
    }
    
    const auto& property = sample.data().delegate().property();
    const auto buffer_id = property.try_get(kBufferIdProperty);
    const auto channel = property.try_get(kChannelProperty);
    const auto guid = property.try_get(kGuidProperty);
    const auto gpu_direct_enabled = property.try_get(kGpuDirectEnabledProperty);
    const auto gpu_direct_fast_dest_ip = property.try_get(kGpuDirectFastDestIpProperty);
    const auto gpu_direct_fast_dest_mac = property.try_get(kGpuDirectFastDestMacProperty);
    const auto gpu_direct_fast_dest_port = property.try_get(kGpuDirectFastDestPortProperty);
    //TODO: Delete this
    
    HOLOSCAN_LOG_INFO("Sample received in sender manager: buffer_id={}, channel={}, guid={}, fast destination ip={}, fast destination mac={}, fast destination port={}",
              buffer_id ? *buffer_id : "null",
              channel ? *channel : "null",
              guid ? *guid : "null",
              gpu_direct_fast_dest_ip ? *gpu_direct_fast_dest_ip : "null",
              gpu_direct_fast_dest_mac ? *gpu_direct_fast_dest_mac : "null",
              gpu_direct_fast_dest_port ? *gpu_direct_fast_dest_port : "null");
    if (!buffer_id || !channel || !guid) {
      HOLOSCAN_LOG_INFO("Skipping receiver registration due to missing or invalid properties");
      continue;
    }

    // Interpret gpu_direct_enabled: only treat as enabled when explicitly set to "true".
    const bool gpu_enabled = (gpu_direct_enabled && *gpu_direct_enabled == "true");

    // Determine whether fast destination fields are present.
    const bool have_fast_dest = (gpu_direct_fast_dest_ip && gpu_direct_fast_dest_mac && gpu_direct_fast_dest_port);

    // If neither gpu_direct is enabled nor fast destination fields are present, skip registration.
    if (!gpu_enabled && !have_fast_dest) {
      HOLOSCAN_LOG_INFO("Skipping receiver registration because gpu_direct is not enabled and no fast_dest info provided");
      continue;
    }

    if (!channel_filter_.empty() && *channel != channel_filter_) {
      continue;
    }

    DestinationInfo destination;

    // If we have fast destination info (either because gpu_direct is enabled, or the fields were provided
    // without the enabled flag), attempt to parse and use them. Parsing is done defensively.
    if (have_fast_dest) {
      destination.ip_addr = *gpu_direct_fast_dest_ip;
      destination.mac_addr = *gpu_direct_fast_dest_mac;
      try {
        const int port = std::stoi(*gpu_direct_fast_dest_port);
        if (port < 0 || port > 0xFFFF) {
          HOLOSCAN_LOG_INFO("Skipping receiver registration due to invalid fast_dest_port value: {}", *gpu_direct_fast_dest_port);
          continue;
        }
        destination.udp_port = static_cast<uint16_t>(port);
      } catch (const std::exception& e) {
        HOLOSCAN_LOG_INFO("Skipping receiver registration due to invalid fast_dest_port parse error: {}", e.what());
        continue;
      }
    } else {
      // No fast destination info available; fall back to skipping registration.
      HOLOSCAN_LOG_INFO("Skipping receiver registration: no fast_dest info available");
      continue;
    }

    registerReceiver(*guid, destination.toString());

  }
}

}  // namespace connext_lib
