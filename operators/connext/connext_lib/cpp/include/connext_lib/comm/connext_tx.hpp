#pragma once
// connext_tx.hpp
// High-level transmitter for Holoscan Connext library.
// Publishes buffers to every registered receiver.

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include "connext_lib/transport/payload_transport.hpp"
#include "connext_lib/resource/resource_managers.hpp"

namespace connext_lib {

/**
 * High-level transmitter that publishes buffers to every registered receiver.
 * Used in tests to broadcast payloads to multiple destinations.
 */
class ConnextTx {
 public:
  /**
   * Constructs a transmitter with sender manager and payload writer.
   * Used in tests for broadcast and single-destination transmission.
   */
  ConnextTx(std::unique_ptr<SenderResourcesManagerInterface> sender_manager,
            std::unique_ptr<PayloadWriterInterface> payload_writer,
            std::chrono::milliseconds poll_interval = std::chrono::milliseconds{100});
  ConnextTx(const ConnextTx&) = delete;
  ConnextTx& operator=(const ConnextTx&) = delete;
  ~ConnextTx();

  /**
   * Stages the payload so it can be broadcast or sent to a single destination.
   */
  void setBuffer(const MemoryBufferView& buffer);

  /**
   * Sends the staged payload to a specific destination.
   * Returns true if successful.
   */
  bool sendTo(const std::string& destination_reference);

  /**
   * Sends the staged payload to every known destination.
   * Returns the number of successful sends.
   */
  std::size_t broadcast();
  
  /**
   * Flushes all pending bursts with timeout.
   * Returns the number of bursts successfully sent.
   */
  int flush(int timeout_ms = 1000);

 private:
  std::unique_ptr<SenderResourcesManagerInterface> sender_manager_;
  std::unique_ptr<PayloadWriterInterface> payload_writer_;
};

}  // namespace connext_lib
