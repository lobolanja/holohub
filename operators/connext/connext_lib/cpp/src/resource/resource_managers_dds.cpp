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
constexpr char kGpuDirectFastDestIpProperty[] = "connext_lib.receiver.gpu_direct.fast_dest_ip";
constexpr char kGpuDirectFastDestMacProperty[] = "connext_lib.receiver.gpu_direct.fast_dest_mac";
constexpr char kGpuDirectFastDestPortProperty[] = "connext_lib.receiver.gpu_direct.fast_dest_port";

std::string GuidToString(const rti::core::Guid& guid) {
  std::ostringstream oss;
  oss << guid;
  return oss.str();
}

std::string DataStateToString(const dds::sub::status::DataState& state) {
  std::ostringstream oss;
  oss << "DataState(";
  
  // Sample State
  oss << "sample_state=";
  if (state.sample_state() == dds::sub::status::SampleState::read()) {
    oss << "READ";
  } else if (state.sample_state() == dds::sub::status::SampleState::not_read()) {
    oss << "NOT_READ";
  } else {
    oss << "UNKNOWN(" << state.sample_state().to_ulong() << ")";
  }
  
  // View State
  oss << ", view_state=";
  if (state.view_state() == dds::sub::status::ViewState::new_view()) {
    oss << "NEW";
  } else if (state.view_state() == dds::sub::status::ViewState::not_new_view()) {
    oss << "NOT_NEW";
  } else {
    oss << "UNKNOWN(" << state.view_state().to_ulong() << ")";
  }
  
  // Instance State
  oss << ", instance_state=";
  if (state.instance_state() == dds::sub::status::InstanceState::alive()) {
    oss << "ALIVE";
  } else if (state.instance_state() == dds::sub::status::InstanceState::not_alive_disposed()) {
    oss << "NOT_ALIVE_DISPOSED";
  } else if (state.instance_state() == dds::sub::status::InstanceState::not_alive_no_writers()) {
    oss << "NOT_ALIVE_NO_WRITERS";
  } else {
    oss << "UNKNOWN(" << state.instance_state().to_ulong() << ")";
  }
  
  oss << ")";
  return oss.str();
}

}  // namespace

namespace connext_lib {

DdsReceiverResourcesManager::DdsReceiverResourcesManager(
    dds::domain::DomainParticipant participant,
    const connext_lib::AnoConfig& config)
    : participant_(std::move(participant)),
      publisher_(participant_),
      topic_(participant_, config.channel_name(), dds::topic::qos::TopicQos()),
      writer_(dds::core::null),
      buffer_id_(config.buffer_id()),
      channel_(topic_.name()),
      ano_config_(config) {

      }

rti::core::policy::Property DdsReceiverResourcesManager::buildProperties()
    const {
  rti::core::policy::Property properties;
  properties.set({kBufferIdProperty, buffer_id_}, true);
  properties.set({kChannelProperty, channel_}, true);
  //properties.set({kGuidProperty, guidString()}, true);
  // Publish fast destination fields for ANO discovery
  const auto& gpu_cfg = ano_config_.ano_network_config();
  properties.set({kGpuDirectFastDestIpProperty, gpu_cfg.fast_ip()}, true);
  properties.set({kGpuDirectFastDestMacProperty, gpu_cfg.fast_mac_address()}, true);
  properties.set({kGpuDirectFastDestPortProperty, std::to_string(gpu_cfg.fast_port())}, true);
  return properties;
}

bool DdsReceiverResourcesManager::announce() {
  if (writer_==dds::core::null) {
    HOLOSCAN_LOG_INFO("Announcing receiver resources: buffer_id={}, channel={}, fast destination ip={}, fast destination mac={}, fast destination port={}",
              buffer_id_, channel_,
              ano_config_.ano_network_config().fast_ip(),
              ano_config_.ano_network_config().fast_mac_address(),
              ano_config_.ano_network_config().fast_port());
    auto qos = applyProperties(buildProperties());
    writer_ = dds::pub::DataWriter<dds::core::BytesTopicType>(publisher_, topic_, qos);
    return true;
  } else {
    //HOLOSCAN_LOG_INFO("Re-announcing receiver resources");
    //auto qos = applyProperties(buildProperties());
    //writer_.qos(qos); // workaround for forcing endpoint discovery again.
    //dds::core::BytesTopicType sample;
    //writer_.write(sample);
    writer_.assert_liveliness();
    return true;
  }
}

dds::pub::qos::DataWriterQos DdsReceiverResourcesManager::applyProperties(
    const ReceiverPropertySet& properties) {

  dds::pub::qos::DataWriterQos qos = dds::core::QosProvider::Default().datawriter_qos(
                  "BuiltinQosLib::Generic.KeepLastReliable.TransientLocal");
  
  auto property_policy = qos.policy<rti::core::policy::Property>();
  const auto entries = properties.get_all();
  for (const auto& entry : entries) {
    property_policy.set(entry, properties.propagate(entry.first));
  }
  qos.policy(property_policy);

  // Configure Liveliness MANUAL_BY_TOPIC with 0.5s lease duration
  qos.policy<dds::core::policy::Liveliness>(dds::core::policy::Liveliness(
      dds::core::policy::LivelinessKind::MANUAL_BY_TOPIC,
      dds::core::Duration::from_millisecs(500)));

  return qos;
}

std::string DdsReceiverResourcesManager::guidString() const {
  auto protocol =
      writer_.qos().policy<rti::core::policy::DataWriterProtocol>();
  return GuidToString(protocol.virtual_guid());
}

DdsSenderResourcesManager::DdsSenderResourcesManager(
    dds::domain::DomainParticipant participant,
    std::string channel)
    : participant_(std::move(participant)),
      subscriber_(participant_),
      topic_(participant_, std::move(channel), dds::topic::qos::TopicQos()),
      reader_(dds::core::null),
      subscription_reader_(dds::core::null),
      channel_filter_(topic_.name()) {
  
  dds::sub::Subscriber builtin = dds::sub::builtin_subscriber(participant_);
  std::vector<
      dds::sub::DataReader<dds::topic::PublicationBuiltinTopicData>>
      readers;
  dds::sub::find<dds::sub::DataReader<dds::topic::PublicationBuiltinTopicData>>(
      builtin, DDS_PUBLICATION_TOPIC_NAME, std::back_inserter(readers));
  HOLOSCAN_LOG_INFO("DdsSenderResourcesManager found {} subscription builtin topic readers", readers.size());
  if (readers.empty()) {
    throw std::runtime_error(
        "Failed to locate subscription builtin topic reader");
  }

  subscription_reader_ = readers[0];

  // Create reader QoS and set Liveliness MANUAL_BY_TOPIC with 0.5s lease duration
  auto reader_qos = dds::core::QosProvider::Default().datareader_qos(
      "BuiltinQosLib::Generic.KeepLastReliable.TransientLocal");
  reader_qos.policy<dds::core::policy::Liveliness>(dds::core::policy::Liveliness(
      dds::core::policy::LivelinessKind::MANUAL_BY_TOPIC,
      dds::core::Duration::from_millisecs(500)));

  // Construct the DataReader with the modified QoS
  reader_ = dds::sub::DataReader<dds::core::BytesTopicType>(subscriber_, topic_, reader_qos);
  
}

void DdsSenderResourcesManager::pollOnce() {
  // Validate DDS entities before attempting to use them
  if (reader_ == dds::core::null) {
    HOLOSCAN_LOG_WARN("DdsSenderResourcesManager: reader_ is null, skipping poll");
    return;
  }
  if (subscription_reader_ == dds::core::null) {
    HOLOSCAN_LOG_WARN("DdsSenderResourcesManager: subscription_reader_ is null, skipping poll");
    return;
  }
  
  //HOLOSCAN_LOG_INFO("Polling for new receiver resources");
  auto dumySamples = reader_.take();
  for (const auto& sample : dumySamples) {
    if (!sample.info().valid()) {
      HOLOSCAN_LOG_INFO("Received sample with invalid info in sender manager reader: state {}", DataStateToString(sample.info().state()));

      if(sample.info().state().instance_state() != dds::sub::status::InstanceState::alive())
      {
        HOLOSCAN_LOG_INFO("Received instance state NOT_ALIVE_NO_WRITERS or NOT_ALIVE_DISPOSED in sender manager reader - this indicates a receiver has stopped announcing resources");

        // Extract the writer GUID from the sample info
        rti::core::Guid writer_guid = sample.info()->publication_virtual_guid();
        HOLOSCAN_LOG_INFO("Writer GUID of the NOT_ALIVE_NO_WRITERS sample is {}: unregistering receiver", GuidToString(writer_guid));
        this->unregisterReceiver(GuidToString(writer_guid));
      }
    }
  }

  auto samples = subscription_reader_.take();
  for (const auto& sample : samples) {
    HOLOSCAN_LOG_INFO("Sample received in sender manager subscription reader");
    if (!sample.info().valid()) {
      HOLOSCAN_LOG_INFO("Received sample with invalid info in sender manager subscription reader: state {}", DataStateToString(sample.info().state()));
      continue;
    }
    
    const auto& property = sample.data().delegate().property();
    const auto buffer_id = property.try_get(kBufferIdProperty);
    const auto channel = property.try_get(kChannelProperty);
    
    // Get GUID from the publication builtin topic data key
 
    std::string guid = GuidToString(sample.data()->virtual_guid());
    
    const auto gpu_direct_fast_dest_ip = property.try_get(kGpuDirectFastDestIpProperty);
    const auto gpu_direct_fast_dest_mac = property.try_get(kGpuDirectFastDestMacProperty);
    const auto gpu_direct_fast_dest_port = property.try_get(kGpuDirectFastDestPortProperty);
    //TODO: Delete this
    
    HOLOSCAN_LOG_INFO("Sample received in sender manager: buffer_id={}, channel={}, guid={}, fast destination ip={}, fast destination mac={}, fast destination port={}",
              buffer_id ? *buffer_id : "null",
              channel ? *channel : "null",
              guid.c_str(),
              gpu_direct_fast_dest_ip ? *gpu_direct_fast_dest_ip : "null",
              gpu_direct_fast_dest_mac ? *gpu_direct_fast_dest_mac : "null",
              gpu_direct_fast_dest_port ? *gpu_direct_fast_dest_port : "null");
    if (!buffer_id || !channel || guid.empty()) {
      HOLOSCAN_LOG_INFO("Skipping receiver registration due to missing or invalid properties");
      continue;
    }

    // Check if fast destination fields are present - this indicates ANO capability
    const bool have_fast_dest = (gpu_direct_fast_dest_ip && gpu_direct_fast_dest_mac && gpu_direct_fast_dest_port);

    // Skip registration if no fast destination info is available
    if (!have_fast_dest) {
      HOLOSCAN_LOG_WARN("Skipping receiver registration: no fast_dest info provided");
      continue;
    }

    if (!channel_filter_.empty() && *channel != channel_filter_) {
      continue;
    }

    DestinationInfo destination;
    destination.ip_addr = *gpu_direct_fast_dest_ip;
    destination.mac_addr = *gpu_direct_fast_dest_mac;
    try {
      const int port = std::stoi(*gpu_direct_fast_dest_port);
      if (port < 0 || port > 0xFFFF) {
        HOLOSCAN_LOG_WARN("Skipping receiver registration due to invalid fast_dest_port value: {}", *gpu_direct_fast_dest_port);
        continue;
      }
      destination.udp_port = static_cast<uint16_t>(port);
    } catch (const std::exception& e) {
      HOLOSCAN_LOG_WARN("Skipping receiver registration due to invalid fast_dest_port parse error: {}", e.what());
      continue;
    }

    registerReceiver(guid, destination.toString());

  }
}

}  // namespace connext_lib
