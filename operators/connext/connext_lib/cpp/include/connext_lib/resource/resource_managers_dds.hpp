#pragma once
// resource_managers_dds.hpp
// DDS-specific resource manager implementations for Holoscan Connext library.
// Provides classes for announcing receiver resources and tracking sender destinations
// using DDS. Used in resource manager tests for DDS discovery and filtering.

#include <string>
#include "connext_lib/resource/resource_managers.hpp"
#include "connext_lib/config/config.hpp"
#include "dds/dds.hpp"
#include "dds/topic/BuiltinTopic.hpp"

namespace connext_lib {

/**
 * DDS implementation of receiver resources manager.
 * Announces receiver buffer references and manages DDS entities for reception.
 * Used in tests to verify receiver discovery and announcement via DDS.
 */
class DdsReceiverResourcesManager : public ReceiverResourcesManagerInterface {
 public:
  DdsReceiverResourcesManager(dds::domain::DomainParticipant participant,
                              const connext_lib::AnoConfig& config);
  /**
   * Announces receiver presence and buffer ID on the specified DDS channel.
   * Returns true if announcement was successful.
   */
  bool announce() override;
 private:
  /**
   * Builds DDS property set for receiver announcement.
   */
  [[nodiscard]] ReceiverPropertySet buildProperties() const;
  /**
   * Applies DDS properties to the receiver entity.
   */
  dds::pub::qos::DataWriterQos applyProperties(const ReceiverPropertySet& properties);
  /**
   * Returns the receiver's DDS GUID as a string.
   */
  [[nodiscard]] std::string guidString() const;
  dds::domain::DomainParticipant participant_;
  dds::pub::Publisher publisher_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::pub::DataWriter<dds::core::BytesTopicType> writer_;
  std::string buffer_id_;
  std::string channel_;
  connext_lib::AnoConfig ano_config_;
};

/**
 * DDS implementation of sender resources manager.
 * Tracks discovered receivers and manages DDS entities for transmission.
 * Used in tests to verify sender discovery, filtering, and polling via DDS.
 */
class DdsSenderResourcesManager : public AbstractSenderResourcesManager {
 public:
  DdsSenderResourcesManager(dds::domain::DomainParticipant participant,
                            std::string channel);
 protected:
  /**
   * Polls DDS for receiver announcements and updates destination list.
   * Called by the polling loop in AbstractSenderResourcesManager.
   */
  void pollOnce() override;
 private:
  dds::domain::DomainParticipant participant_;
  dds::sub::Subscriber subscriber_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::sub::DataReader<dds::core::BytesTopicType> reader_;
  dds::sub::DataReader<dds::topic::PublicationBuiltinTopicData> subscription_reader_;
  std::string channel_filter_;
};

}  // namespace connext_lib
