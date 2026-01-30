#pragma once
// connext_rx.hpp
// High-level receiver helper for Holoscan Connext library.
// Wires resource announcements to the payload transport reader.

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>
#include "connext_lib/transport/payload_transport.hpp"
#include "connext_lib/resource/resource_managers.hpp"

namespace connext_lib {

/**
 * High-level receiver helper that wires resource announcements to the
 * payload transport reader. Used in tests to receive payloads from DDS/ANO.
 */
class ConnextRx {
 public:
  /**
   * Constructs a receiver with resource manager and payload reader.
   * Used in tests for roundtrip and resource announcement validation.
   */
  ConnextRx(std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager,
            std::unique_ptr<PayloadReaderInterface> payload_reader);
  ConnextRx(const ConnextRx&) = delete;
  ConnextRx& operator=(const ConnextRx&) = delete;
  ~ConnextRx();

  /**
   * Blocks until a payload arrives or the timeout expires.
   * Returns MemoryBufferView with ptr and size on success.
   * Returns {nullptr, 0, false} on timeout or failure.
   * Caller must call freeBuffer() to release the returned buffer.
   * Used in tests to verify correct reception and timeout handling.
   */
  MemoryBufferView receive(std::chrono::milliseconds timeout);

  /**
   * Frees a buffer previously returned by receive().
   * Must be called to release memory allocated by the payload reader.
   */
  void freeBuffer(const MemoryBufferView& buffer);

 private:
  std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager_;
  std::unique_ptr<PayloadReaderInterface> payload_reader_;
};

}  // namespace connext_lib
