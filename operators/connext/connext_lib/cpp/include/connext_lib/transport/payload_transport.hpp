#pragma once
// payload_transport.hpp
// Generic payload transport interfaces for Holoscan Connext library.
// Provides abstractions for buffer views, writer/reader options, and transport factories.
// Used in tests to validate payload handling, option propagation, and interface contracts.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace connext_lib {

/**
 * Immutable view into a contiguous payload buffer.
 * Used by writers to stage data and by readers to access received payloads.
 * Does not own memory; caller is responsible for buffer lifetime.
 */
struct PayloadBufferView {
  const std::uint8_t* data{nullptr};
  std::size_t size_bytes{0};
};

/**
 * Options for configuring a payload writer instance.
 * Includes logical channel and maximum payload size.
 * Used in tests to verify option propagation and defaults.
 */
struct PayloadWriterOptions {
  std::string channel;
  std::size_t max_payload_bytes{0};
};

/**
 * Options for configuring a payload reader instance.
 * Includes logical channel, expected payload size, and buffer ID.
 * Used in tests to verify option propagation and defaults.
 */
struct PayloadReaderOptions {
  std::string channel;
  std::size_t expected_payload_bytes{0};
  std::string buffer_id;
};

/**
 * Interface for payload writers.
 * Writers stage payloads and send them to a destination.
 * Used in tests to verify staging, sending, and metadata propagation.
 */
class PayloadWriterInterface {
 public:
  virtual ~PayloadWriterInterface() = default;
  /**
   * Stages the supplied payload for later transmission.
   */
  virtual void setBuffer(const PayloadBufferView& buffer) = 0;
  /**
   * Sends the staged payload to the specified destination.
   * Returns true if successful.
   */
  virtual bool writeTo(const std::string& destination_reference) = 0;
};

/**
 * Interface for payload readers.
 * Readers receive payloads and write them into caller-provided storage.
 * Used in tests to verify delivery and timeout behavior.
 */
class PayloadReaderInterface {
 public:
  virtual ~PayloadReaderInterface() = default;
  /**
   * Blocks until data arrives or timeout expires, writing bytes into output.
   * Returns true if data was received.
   */
  virtual bool readNext(std::vector<std::uint8_t>& destination,
                        std::chrono::milliseconds timeout) = 0;
};

/**
 * Abstract factory for creating transport-specific writers and readers.
 * Used in tests to verify option propagation and interface contracts.
 */
class PayloadTransport {
 public:
  virtual ~PayloadTransport() = default;
  /**
   * Creates a payload writer with the given options.
   */
  virtual std::unique_ptr<PayloadWriterInterface> createWriter(
      const PayloadWriterOptions& options) = 0;
  /**
   * Creates a payload reader with the given options.
   */
  virtual std::unique_ptr<PayloadReaderInterface> createReader(
      const PayloadReaderOptions& options) = 0;
};

}  // namespace connext_lib
