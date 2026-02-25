#include <connext_lib/resource/resource_managers_idl.hpp>
#include "ResourceAnnouncement.hpp"
#include <holoscan/logger/logger.hpp>
#include "rti/core/Guid.hpp"
#include "rti/core/policy/CorePolicy.hpp"
#include "dds/core/QosProvider.hpp"
#include <sstream>
#include <stdexcept>

namespace {

constexpr int kDiscoveryDomainId = 101;
constexpr char kDiscoveryTopicName[] = "GPUResourceDiscovery";

std::string GuidToString(const rti::core::Guid& guid) {
  std::ostringstream oss;
  oss << guid;
  return oss.str();
}

}  // namespace

namespace connext_lib {

// ============================================================================
// DdsIdlReceiverResourcesManager Implementation
// ============================================================================

DdsIdlReceiverResourcesManager::DdsIdlReceiverResourcesManager(
    dds::domain::DomainParticipant participant,
    const AnoConfig& config)
    : participant_(std::move(participant)),
      discovery_participant_(kDiscoveryDomainId),
      publisher_(dds::core::null),
      topic_(dds::core::null),
      writer_(dds::core::null),
      buffer_id_(),
      channel_(config.channel_name()),
      ano_config_(config),
      first_announce_(true) {
  
  // Create topic on dedicated discovery domain
  topic_ = dds::topic::Topic<ResourceAnnouncement>(
      discovery_participant_, kDiscoveryTopicName);
  
  HOLOSCAN_LOG_INFO("DdsIdlReceiverResourcesManager created on domain {}", 
      kDiscoveryDomainId);
}

DdsIdlReceiverResourcesManager::~DdsIdlReceiverResourcesManager() {
  if (writer_ != dds::core::null) {
    try {
      sendSample(RegistrationAction::UNREGISTER);
      HOLOSCAN_LOG_INFO("Sent UNREGISTER for buffer_id={}", buffer_id_);
    } catch (const std::exception& e) {
      HOLOSCAN_LOG_ERROR("Failed to send UNREGISTER: {}", e.what());
    }
  }
}

bool DdsIdlReceiverResourcesManager::announce() {
  if (first_announce_) {
    // First announcement: create writer and send REGISTER sample
    auto qos = createWriterQos();
    publisher_ = dds::pub::Publisher(discovery_participant_);
    writer_ = dds::pub::DataWriter<ResourceAnnouncement>(
        publisher_, topic_, qos);
    
    // Get GUID after writer creation
    buffer_id_ = getWriterGuid();
    
    sendSample(RegistrationAction::REGISTER);
    
    HOLOSCAN_LOG_INFO(
        "Announced receiver: buffer_id={}, channel={}, ip={}, mac={}, port={}",
        buffer_id_, channel_,
        ano_config_.ano_network_config().fast_ip(),
        ano_config_.ano_network_config().fast_mac_address(),
        ano_config_.ano_network_config().fast_port());
    
    first_announce_ = false;
    return true;
  }
  
  // Subsequent announcements: only assert liveliness
  writer_.assert_liveliness();
  HOLOSCAN_LOG_DEBUG("Re-announced receiver via liveliness");
  return true;
}

void DdsIdlReceiverResourcesManager::sendSample(RegistrationAction action) {
  if (writer_ == dds::core::null) {
    throw std::runtime_error("Writer not initialized");
  }
  
  ResourceAnnouncement sample;
  sample.buffer_id(buffer_id_);
  sample.channel(channel_);
  sample.fast_dest_ip(ano_config_.ano_network_config().fast_ip());
  sample.fast_dest_mac(ano_config_.ano_network_config().fast_mac_address());
  sample.fast_dest_port(ano_config_.ano_network_config().fast_port());
  sample.action(action);
  
  writer_.write(sample);
}

dds::pub::qos::DataWriterQos DdsIdlReceiverResourcesManager::createWriterQos() const {
  auto qos = dds::core::QosProvider::Default().datawriter_qos(
      "BuiltinQosLib::Generic.KeepLastReliable.TransientLocal");
  
  qos.policy<dds::core::policy::History>(
      dds::core::policy::History::KeepLast(1));
  
  qos.policy<dds::core::policy::Liveliness>(
      dds::core::policy::Liveliness(
          dds::core::policy::LivelinessKind::MANUAL_BY_TOPIC,
          dds::core::Duration::from_millisecs(500)));
  
  return qos;
}

std::string DdsIdlReceiverResourcesManager::getWriterGuid() const {
  if (writer_ == dds::core::null) {
    throw std::runtime_error("Writer not initialized");
  }
  auto protocol = writer_.qos().policy<rti::core::policy::DataWriterProtocol>();
  return GuidToString(protocol.virtual_guid());
}

// ============================================================================
// DdsIdlSenderResourcesManager Implementation
// ============================================================================

DdsIdlSenderResourcesManager::DdsIdlSenderResourcesManager(
    dds::domain::DomainParticipant participant,
    std::string channel_filter)
    : participant_(std::move(participant)),
      discovery_participant_(kDiscoveryDomainId),
      subscriber_(dds::core::null),
      topic_(dds::core::null),
      reader_(dds::core::null),
      channel_filter_(std::move(channel_filter)) {
  
  // Create topic on dedicated discovery domain
  topic_ = dds::topic::Topic<ResourceAnnouncement>(
      discovery_participant_, kDiscoveryTopicName);
  
  auto qos = createReaderQos();
  subscriber_ = dds::sub::Subscriber(discovery_participant_);
  reader_ = dds::sub::DataReader<ResourceAnnouncement>(
      subscriber_, topic_, qos);
  
  HOLOSCAN_LOG_INFO("DdsIdlSenderResourcesManager initialized on domain {}",
      kDiscoveryDomainId);
}

void DdsIdlSenderResourcesManager::pollOnce() {
  if (reader_ == dds::core::null) {
    HOLOSCAN_LOG_WARN("Reader is null, skipping poll");
    return;
  }
  
  auto samples = reader_.take();
  for (const auto& sample : samples) {
    if (sample.info().valid()) {
      processSample(sample);
    } else {
      handleNotAliveInstance(sample.info());
    }
  }
}

void DdsIdlSenderResourcesManager::processSample(
    const rti::sub::LoanedSample<ResourceAnnouncement>& sample) {
  const auto& data = sample.data();
  
  HOLOSCAN_LOG_INFO("Received sample: buffer_id={}, channel={}, action={}",
      data.buffer_id(), data.channel(),
      data.action() == RegistrationAction::REGISTER ? "REGISTER" : "UNREGISTER");
  
  // Apply channel filter if specified
  if (!channel_filter_.empty() && data.channel() != channel_filter_) {
    HOLOSCAN_LOG_INFO("Skipping sample: channel filter mismatch");
    return;
  }
  
  DestinationInfo destination;
  destination.ip_addr = data.fast_dest_ip();
  destination.mac_addr = data.fast_dest_mac();
  destination.udp_port = data.fast_dest_port();
  
  if (data.action() == RegistrationAction::REGISTER) {
    registerReceiver(data.buffer_id(), destination.toString());
    HOLOSCAN_LOG_INFO("Registered receiver: buffer_id={}, dest={}",
        data.buffer_id(), destination.toString());
  } else {
    unregisterReceiver(data.buffer_id());
    HOLOSCAN_LOG_INFO("Unregistered receiver: buffer_id={}", data.buffer_id());
  }
}

void DdsIdlSenderResourcesManager::handleNotAliveInstance(
    const dds::sub::SampleInfo& info) {
  const auto& state = info.state();
  
  if (state.instance_state() != dds::sub::status::InstanceState::alive()) {
    rti::core::Guid writer_guid = info->publication_virtual_guid();
    std::string guid_str = GuidToString(writer_guid);
    
    HOLOSCAN_LOG_INFO(
        "Instance NOT_ALIVE detected, unregistering receiver with GUID={}",
        guid_str);
    
    unregisterReceiver(guid_str);
  }
}

dds::sub::qos::DataReaderQos DdsIdlSenderResourcesManager::createReaderQos() const {
  auto qos = dds::core::QosProvider::Default().datareader_qos(
      "BuiltinQosLib::Generic.KeepLastReliable.TransientLocal");
  
  qos.policy<dds::core::policy::History>(
      dds::core::policy::History::KeepLast(1));
  
  qos.policy<dds::core::policy::Liveliness>(
      dds::core::policy::Liveliness(
          dds::core::policy::LivelinessKind::MANUAL_BY_TOPIC,
          dds::core::Duration::from_millisecs(500)));
  
  return qos;
}

}  // namespace connext_lib
