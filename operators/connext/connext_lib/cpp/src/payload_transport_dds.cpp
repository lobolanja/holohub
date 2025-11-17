#include "connext_lib/payload_transport_dds.hpp"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

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

DdsPayloadWriter::DdsPayloadWriter(int domain_id,
                                   std::string topic_name,
                                   std::size_t max_payload_bytes)
    : participant_(domain_id),
      topic_(participant_,
             std::move(topic_name),
             dds::topic::qos::TopicQos()),
      publisher_(participant_),
      writer_(publisher_,
              topic_,
              dds::core::QosProvider::Default().datawriter_qos(
                  "BuiltinQosLib::Pattern.Status")),
      max_payload_bytes_(max_payload_bytes) {}

void DdsPayloadWriter::SetBuffer(const PayloadBufferView& buffer) {
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

bool DdsPayloadWriter::WriteTo(const std::string& /*destination_reference*/) {
  // DDS topics are multicast by default; destination_reference is ignored. Because dds will handle dicvoery matchhing
  try {
    dds::core::BytesTopicType sample(payload_);
    writer_.write(sample);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

DdsPayloadReader::DdsPayloadReader(int domain_id, std::string topic_name)
    : participant_(domain_id),
      topic_(participant_,
             std::move(topic_name),
             dds::topic::qos::TopicQos()),
      subscriber_(participant_),
      reader_(subscriber_,
              topic_,
              dds::core::QosProvider::Default().datareader_qos(
                  "BuiltinQosLib::Pattern.Status")) {}

bool DdsPayloadReader::ReadNext(std::vector<std::uint8_t>& destination,
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
std::unique_ptr<PayloadWriterInterface> MakeDdsPayloadWriter(
    int domain_id,
    const PayloadWriterOptions& options) {
  return std::make_unique<DdsPayloadWriter>(
      domain_id, ResolveChannel(options.channel), options.max_payload_bytes);
}

std::unique_ptr<PayloadReaderInterface> MakeDdsPayloadReader(
    int domain_id,
    const PayloadReaderOptions& options) {
  return std::make_unique<DdsPayloadReader>(
      domain_id, ResolveChannel(options.channel));
}

DdsPayloadTransport::DdsPayloadTransport(int domain_id)
    : domain_id_(domain_id) {}

DdsPayloadTransport::~DdsPayloadTransport() = default;

std::unique_ptr<PayloadWriterInterface> DdsPayloadTransport::CreateWriter(
    const PayloadWriterOptions& options) {
  return MakeDdsPayloadWriter(domain_id_, options);
}

std::unique_ptr<PayloadReaderInterface> DdsPayloadTransport::CreateReader(
    const PayloadReaderOptions& options) {
  return MakeDdsPayloadReader(domain_id_, options);
}

}  // namespace connext_lib
