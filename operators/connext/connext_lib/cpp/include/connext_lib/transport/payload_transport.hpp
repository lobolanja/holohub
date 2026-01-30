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
 * View into a memory buffer (CPU or GPU).
 * Used by readers to return received payloads and by writers to reference data to send.
 * Does not own memory; caller is responsible for buffer lifetime and must call freeBuffer().
 */
struct MemoryBufferView {
  void* ptr{nullptr};          // Pointer to buffer (CPU or GPU memory)
  std::uint64_t size_bytes{0}; // Size of buffer in bytes
  bool is_device{false};       // True if ptr points to GPU memory, false for CPU
};

/**
 * Immutable view into a contiguous payload buffer.
 * Used by writers to stage data and by readers to access received payloads.
 * Does not own memory; caller is responsible for buffer lifetime.
 */
struct PayloadBufferView {
  //TODO: delete this data pointer
  const std::uint8_t* data{nullptr};
  std::size_t size_bytes=0;
 
  // If sending from device memory (GPU), set `is_device` to true and
  // provide a CUDA device pointer in `device_ptr` (caller-owned).
  void* device_ptr{nullptr};
  
  
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
   * Blocks until data arrives or timeout expires.
   * On success returns true and sets `data_ptr` to an allocated buffer
   * (host or device pointer) and `size` to the payload length. The caller
   * MUST call `freeData(data_ptr)` to release the returned buffer. On
   * failure or timeout, returns false and `data_ptr` is unspecified.
   */
  virtual bool readNext(void*& data_ptr, std::size_t& size,
                        std::chrono::milliseconds timeout) = 0;

  /**
   * Free a pointer previously returned by `readNext`.
   * Implementations must release memory appropriately (e.g. `delete[]`
   * for DDS host buffers or call `IGpuDirectNetworkReceiver::free_received_data`
   * for ANO GPU buffers).
   */
  virtual void freeData(void* data_ptr) = 0;
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
