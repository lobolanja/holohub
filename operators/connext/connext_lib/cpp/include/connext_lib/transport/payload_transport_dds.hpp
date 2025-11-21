#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "connext_lib/transport/payload_transport.hpp"
#include "dds/dds.hpp"
#include <dds/domain/DomainParticipant.hpp>
namespace connext_lib {
class DdsPayloadWriter : public PayloadWriterInterface {
 public:
  DdsPayloadWriter(dds::domain::DomainParticipant& participant,
                   const std::string& topic_name,
                   std::size_t max_payload_bytes);
  void setBuffer(const PayloadBufferView& buffer) override;
  bool writeTo(const std::string& destination_reference) override;
  dds::pub::DataWriter<dds::core::BytesTopicType>& get_dds_writer() { return writer_; }
 private:
  dds::domain::DomainParticipant& participant_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::pub::Publisher publisher_;
  dds::pub::DataWriter<dds::core::BytesTopicType> writer_;
  std::size_t max_payload_bytes_{0};
  std::vector<uint8_t> payload_;
};
class DdsPayloadReader : public PayloadReaderInterface {
 public:
  DdsPayloadReader(dds::domain::DomainParticipant& participant,
                   const std::string& topic_name,
                   const std::string& destination_reference);
  bool readNext(std::vector<std::uint8_t>& destination,
                std::chrono::milliseconds timeout) override;
  dds::sub::DataReader<dds::core::BytesTopicType>& get_dds_reader() { return reader_; }
 private:
  dds::domain::DomainParticipant& participant_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::sub::Subscriber subscriber_;
  dds::sub::DataReader<dds::core::BytesTopicType> reader_;
};
class DdsPayloadTransport : public PayloadTransport {
 public:
  explicit DdsPayloadTransport(dds::domain::DomainParticipant& dp);
  ~DdsPayloadTransport() override;
  std::unique_ptr<PayloadWriterInterface> createWriter(
      const PayloadWriterOptions& options) override;
  std::unique_ptr<PayloadReaderInterface> createReader(
      const PayloadReaderOptions& options) override;
 private:
  dds::domain::DomainParticipant domain_participant_;
};
std::unique_ptr<PayloadWriterInterface> MakeDdsPayloadWriter(
    int domain_id, const PayloadWriterOptions& options);
std::unique_ptr<PayloadReaderInterface> MakeDdsPayloadReader(
    int domain_id, const PayloadReaderOptions& options);
}  // namespace connext_lib
