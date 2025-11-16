#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "connext_lib/payload_transport.hpp"
#include "dds/dds.hpp"

namespace connext_lib {

/** Concrete writer that stages payload bytes and publishes them via DDS. */
class DdsPayloadWriter : public PayloadWriterInterface {
 public:
  DdsPayloadWriter(int domain_id,
                   std::string topic_name,
                   std::size_t max_payload_bytes);

  void SetBuffer(const PayloadBufferView& buffer) override;
  bool WriteTo(const std::string& destination_reference) override;

 private:
  dds::domain::DomainParticipant participant_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::pub::Publisher publisher_;
  dds::pub::DataWriter<dds::core::BytesTopicType> writer_;
  std::size_t max_payload_bytes_{0};
  std::vector<uint8_t> payload_;
};

/** DDS reader that blocks until a valid sample is available. */
class DdsPayloadReader : public PayloadReaderInterface {
 public:
  DdsPayloadReader(int domain_id, std::string topic_name);

  bool ReadNext(std::vector<std::uint8_t>& destination,
                std::chrono::milliseconds timeout) override;

 private:
  dds::domain::DomainParticipant participant_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::sub::Subscriber subscriber_;
  dds::sub::DataReader<dds::core::BytesTopicType> reader_;
};

/** Factory that instantiates DDS-based payload writers/readers. */
class DdsPayloadTransport : public PayloadTransport {
 public:
  explicit DdsPayloadTransport(int domain_id);
  ~DdsPayloadTransport() override;

  std::unique_ptr<PayloadWriterInterface> CreateWriter(
      const PayloadWriterOptions& options) override;

  std::unique_ptr<PayloadReaderInterface> CreateReader(
      const PayloadReaderOptions& options) override;

 private:
  int domain_id_{0};
};

std::unique_ptr<PayloadWriterInterface> MakeDdsPayloadWriter(
    int domain_id, const PayloadWriterOptions& options);

std::unique_ptr<PayloadReaderInterface> MakeDdsPayloadReader(
    int domain_id, const PayloadReaderOptions& options);

}  // namespace connext_lib
