#pragma once

#include <connext_lib/resource/resource_managers.hpp>
#include <connext_lib/config/config.hpp>
#include "dds/domain/DomainParticipant.hpp"
#include "dds/pub/DataWriter.hpp"
#include "dds/sub/DataReader.hpp"
#include "dds/topic/Topic.hpp"
#include <string>

namespace connext_lib {

// Forward declaration of generated IDL type
class ResourceAnnouncement;
enum class RegistrationAction;

// Receiver: announces resources via IDL samples
class DdsIdlReceiverResourcesManager : public ReceiverResourcesManagerInterface {
public:
    DdsIdlReceiverResourcesManager(
        dds::domain::DomainParticipant participant,
        const AnoConfig& config);
    
    ~DdsIdlReceiverResourcesManager() override;
    
    bool announce() override;

private:
    void sendSample(RegistrationAction action);
    dds::pub::qos::DataWriterQos createWriterQos() const;
    std::string getWriterGuid() const;
    
    dds::domain::DomainParticipant participant_;
    dds::domain::DomainParticipant discovery_participant_;
    dds::pub::Publisher publisher_;
    dds::topic::Topic<ResourceAnnouncement> topic_;
    dds::pub::DataWriter<ResourceAnnouncement> writer_;
    
    std::string buffer_id_;
    std::string channel_;
    AnoConfig ano_config_;
    bool first_announce_;
};

// Sender: discovers resources from IDL samples
class DdsIdlSenderResourcesManager : public AbstractSenderResourcesManager {
public:
    DdsIdlSenderResourcesManager(
        dds::domain::DomainParticipant participant,
        std::string channel_filter = "");
    
    void pollOnce() override;

private:
    void processSample(const rti::sub::LoanedSample<ResourceAnnouncement>& sample);
    void handleNotAliveInstance(const dds::sub::SampleInfo& info);
    dds::sub::qos::DataReaderQos createReaderQos() const;
    
    dds::domain::DomainParticipant participant_;
    dds::domain::DomainParticipant discovery_participant_;
    dds::sub::Subscriber subscriber_;
    dds::topic::Topic<ResourceAnnouncement> topic_;
    dds::sub::DataReader<ResourceAnnouncement> reader_;
    
    std::string channel_filter_;
};

}  // namespace connext_lib
