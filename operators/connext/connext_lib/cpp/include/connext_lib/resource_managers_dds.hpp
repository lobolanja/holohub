#pragma once

#include <string>

#include "connext_lib/resource_managers.hpp"

#include "dds/dds.hpp"
#include "dds/topic/BuiltinTopic.hpp"

namespace connext_lib {

class DdsReceiverResourcesManager : public ReceiverResourcesManagerInterface {
 public:
  DdsReceiverResourcesManager(dds::domain::DomainParticipant participant,
                              std::string buffer_id,
                              std::string channel);

  ReceiverPropertySet BuildProperties() const override;
  bool ApplyProperties(const ReceiverPropertySet& properties) override;

 private:
  std::string GuidString() const;

  dds::domain::DomainParticipant participant_;
  dds::sub::Subscriber subscriber_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::sub::DataReader<dds::core::BytesTopicType> reader_;
  std::string buffer_id_;
  std::string channel_;
};

class DdsSenderResourcesManager : public AbstractSenderResourcesManager {
 public:
  DdsSenderResourcesManager(dds::domain::DomainParticipant participant,
                            std::string channel);

 protected:
  void PollOnce() override;

 private:
  dds::domain::DomainParticipant participant_;
  dds::pub::Publisher publisher_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::pub::DataWriter<dds::core::BytesTopicType> writer_;
  dds::sub::DataReader<dds::topic::SubscriptionBuiltinTopicData> subscription_reader_;
  std::string channel_filter_;
};

}  // namespace connext_lib
