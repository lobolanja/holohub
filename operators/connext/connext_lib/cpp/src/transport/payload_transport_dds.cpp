#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/transport/payload_transport.hpp"
#include <memory>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <thread>

#include "dds/core/QosProvider.hpp"

namespace {

std::string ResolveChannel(const std::string& channel) {
  if (!channel.empty()) {
    return channel;
  }
  return "connext_lib_payload";
}

}  // namespace

namespace connext_lib {

DdsPayloadWriter::DdsPayloadWriter(dds::domain::DomainParticipant& participant,
                                   const std::string& topic_name,
                                   std::size_t max_payload_bytes)
    : participant_(participant),
      topic_(participant_,
             topic_name,
             dds::topic::qos::TopicQos()),
      publisher_(participant_),
      writer_(publisher_,
              topic_,
              dds::core::QosProvider::Default().datawriter_qos(
                  "BuiltinQosLib::Pattern.Status")),
      max_payload_bytes_(max_payload_bytes) {}

void DdsPayloadWriter::setBuffer(const PayloadBufferView& buffer) {
  if (buffer.data == nullptr || buffer.size_bytes == 0) {
    payload_.clear();
    return;
  }
  // If max_payload_bytes_ is zero, there is no limit.
  if (max_payload_bytes_ > 0 && buffer.size_bytes > max_payload_bytes_) {
    throw std::runtime_error("DDS payload exceeds configured maximum");
  }
  payload_.assign(buffer.data, buffer.data + buffer.size_bytes);
}

bool DdsPayloadWriter::writeTo(const std::string& destination_reference) {
  // DDS topics are multicast by default; using partitions to emulate
  try {
    // Get current publisher QoS
    auto pub_qos = publisher_.qos();

    // Set the partition
    // if (!destination_reference.empty()) {
    //   pub_qos << dds::core::policy::Partition(destination_reference);
    // } else {
    //   pub_qos << dds::core::policy::Partition("*");  // Clear partitions
    // }

    pub_qos << dds::core::policy::Partition("*");

    // Apply the new QoS to the publisher
    publisher_.qos(pub_qos);

    // Write the sample
    dds::core::BytesTopicType sample(payload_);
    writer_.write(sample);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

DdsPayloadReader::DdsPayloadReader(dds::domain::DomainParticipant& participant, const std::string& topic_name, const std::string& destination_reference)
    : participant_(participant),
      topic_(participant_,
             topic_name,
             dds::topic::qos::TopicQos()),
      subscriber_(participant_,
                  dds::sub::qos::SubscriberQos() << dds::core::policy::Partition(destination_reference)),
      reader_(subscriber_,
              topic_,
              dds::core::QosProvider::Default().datareader_qos(
                  "BuiltinQosLib::Pattern.Status")) {}

bool DdsPayloadReader::readNext(std::vector<std::uint8_t>& destination,
                                std::chrono::milliseconds timeout) {

  // TODO: this method will take(consume) the data, so we will lost samples if
  // they are not read in time. Consider using read() with sample state
  // filtering and a DataReaderListener to cache samples for later retrieval.
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    try {
      const auto samples = reader_.take();
      for (const auto& sample : samples) {
        if (sample.info().valid()) {
          const std::vector<uint8_t> data = sample.data().data();
          destination = data;
          return true;
        }
      }
    } catch (const std::exception&) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}


// TODO: We can remove the MakeDdsPayloadWriter/Reader functions and move their
// logic directly into the DdsPayloadTransport methods.
std::unique_ptr<PayloadWriterInterface> MakeDdsPayloadWriter(dds::domain::DomainParticipant& dp,
    const PayloadWriterOptions& options) {
  return std::make_unique<DdsPayloadWriter>(
      dp, ResolveChannel(options.channel), options.max_payload_bytes);
}

std::unique_ptr<PayloadReaderInterface> MakeDdsPayloadReader(
    dds::domain::DomainParticipant& dp,
    const PayloadReaderOptions& options) {
  return std::make_unique<DdsPayloadReader>(
      dp, ResolveChannel(options.channel), options.buffer_id);
}

DdsPayloadTransport::DdsPayloadTransport(dds::domain::DomainParticipant& dp)
    : domain_participant_(dp) {}

DdsPayloadTransport::~DdsPayloadTransport() = default;

std::unique_ptr<PayloadWriterInterface> DdsPayloadTransport::createWriter(
    const PayloadWriterOptions& options) {
  return MakeDdsPayloadWriter(domain_participant_, options);
}

std::unique_ptr<PayloadReaderInterface> DdsPayloadTransport::createReader(
    const PayloadReaderOptions& options) {
  return MakeDdsPayloadReader(domain_participant_, options);
}

}  // namespace connext_lib
