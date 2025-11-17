#include "connext_lib/resource_managers_dds.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

#include "dds/core/QosProvider.hpp"
#include "dds/sub/find.hpp"
#include "ndds/dds_c/dds_c_builtin.h"
#include "rti/core/Guid.hpp"
#include "rti/core/policy/CorePolicy.hpp"
#include "ndds/hpp/rti/topic/BuiltinTopicImpl.hpp"

namespace {

constexpr char kBufferIdProperty[] = "connext_lib.receiver.buffer_id";
constexpr char kChannelProperty[] = "connext_lib.receiver.channel";
constexpr char kGuidProperty[] = "connext_lib.receiver.writer_guid";

std::string GuidToString(const rti::core::Guid& guid) {
  std::ostringstream oss;
  oss << guid;
  return oss.str();
}

}  // namespace

namespace connext_lib {

DdsReceiverResourcesManager::DdsReceiverResourcesManager(
    dds::domain::DomainParticipant participant,
    std::string buffer_id,
    std::string channel)
    : participant_(std::move(participant)),
      subscriber_(participant_),
      topic_(participant_, std::move(channel), dds::topic::qos::TopicQos()),
      reader_(subscriber_, topic_),
      buffer_id_(std::move(buffer_id)),
      channel_(topic_.name()) {}

rti::core::policy::Property DdsReceiverResourcesManager::BuildProperties()
    const {
  rti::core::policy::Property properties;
  properties.set({kBufferIdProperty, buffer_id_}, true);
  properties.set({kChannelProperty, channel_}, true);
  properties.set({kGuidProperty, GuidString()}, true);
  return properties;
}

bool DdsReceiverResourcesManager::ApplyProperties(
    const ReceiverPropertySet& properties) {
  auto qos = reader_.qos();
  auto property_policy = qos.policy<rti::core::policy::Property>();
  const auto entries = properties.get_all();
  for (const auto& entry : entries) {
    property_policy.set(entry, properties.propagate(entry.first));
  }
  reader_.qos(qos);
  return true;
}

std::string DdsReceiverResourcesManager::GuidString() const {
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
                  "BuiltinQosLib::Pattern.Status")),
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

void DdsSenderResourcesManager::PollOnce() {
  auto samples = subscription_reader_.take();
  for (const auto& sample : samples) {
    if (!sample.info().valid()) {
      continue;
    }

    const auto& property = sample.data().delegate().property();
    const auto buffer_id = property.try_get(kBufferIdProperty);
    const auto channel = property.try_get(kChannelProperty);
    const auto guid = property.try_get(kGuidProperty);
    if (!buffer_id || !channel || !guid) {
      continue;
    }
    if (!channel_filter_.empty() && *channel != channel_filter_) {
      continue;
    }
    RegisterReceiver(*guid, *buffer_id);
  }
}

}  // namespace connext_lib
