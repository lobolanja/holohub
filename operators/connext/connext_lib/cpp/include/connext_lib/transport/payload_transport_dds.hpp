#pragma once
// payload_transport_dds.hpp
// DDS-specific payload transport implementations for Holoscan Connext library.
// Provides concrete classes for writing and reading payloads via DDS, and a factory for DDS transport.
// Used in tests to validate DDS roundtrip, timeout, and option propagation.

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "connext_lib/transport/payload_transport.hpp"
#include "dds/dds.hpp"
#include <dds/domain/DomainParticipant.hpp>

namespace connext_lib {

/**
 * DDS implementation of a payload writer.
 * Stages and sends payloads to DDS topics.
 * Used in tests to verify correct transmission and discovery.
 */
class DdsPayloadWriter : public PayloadWriterInterface {
 public:
  DdsPayloadWriter(dds::domain::DomainParticipant& participant,
                   const std::string& topic_name,
                   std::size_t max_payload_bytes);
  void setBuffer(const PayloadBufferView& buffer) override;
  bool writeTo(const std::string& destination_reference) override;
  /**
   * Returns the underlying DDS DataWriter for test discovery helpers.
   */
  dds::pub::DataWriter<dds::core::BytesTopicType>& get_dds_writer() { return writer_; }
 private:
  dds::domain::DomainParticipant& participant_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::pub::Publisher publisher_;
  dds::pub::DataWriter<dds::core::BytesTopicType> writer_;
  std::size_t max_payload_bytes_{0};
  std::vector<uint8_t> payload_;
};

/**
 * DDS implementation of a payload reader.
 * Receives payloads from DDS topics and writes them into caller-provided storage.
 * Used in tests to verify correct reception and timeout behavior.
 */
class DdsPayloadReader : public PayloadReaderInterface {
 public:
  DdsPayloadReader(dds::domain::DomainParticipant& participant,
                   const std::string& topic_name,
                   const std::string& destination_reference);
  bool readNext(std::vector<std::uint8_t>& destination,
                std::chrono::milliseconds timeout) override;
  /**
   * Returns the underlying DDS DataReader for test discovery helpers.
   */
  dds::sub::DataReader<dds::core::BytesTopicType>& get_dds_reader() { return reader_; }
 private:
  dds::domain::DomainParticipant& participant_;
  dds::topic::Topic<dds::core::BytesTopicType> topic_;
  dds::sub::Subscriber subscriber_;
  dds::sub::DataReader<dds::core::BytesTopicType> reader_;
};

/**
 * DDS-specific transport factory for creating payload writers and readers.
 * Used in tests to verify option propagation and correct instantiation.
 */
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

/**
 * Helper to create a DDS payload writer for a given domain and options.
 */
std::unique_ptr<PayloadWriterInterface> MakeDdsPayloadWriter(
    int domain_id, const PayloadWriterOptions& options);
/**
 * Helper to create a DDS payload reader for a given domain and options.
 */
std::unique_ptr<PayloadReaderInterface> MakeDdsPayloadReader(
    int domain_id, const PayloadReaderOptions& options);

}  // namespace connext_lib
