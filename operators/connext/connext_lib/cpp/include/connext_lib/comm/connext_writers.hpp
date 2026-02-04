#pragma once
// connext_writers.hpp
// High-level writer interfaces for Holoscan Connext library.
// Provides classes to broadcast payloads using DDS and ANO transports.

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "connext_lib/config/config.hpp"
#include "connext_lib/comm/connext_tx.hpp"
#include "connext_lib/transport/payload_transport.hpp"
#include "dds/dds.hpp"
#include "dds/dds.hpp"

namespace connext_lib {

/**
 * ANO Writer for broadcasting payloads.
 * Wraps an ANO transmitter and configuration, providing a simple interface
 * to broadcast payloads to all known destinations. Used in ANO tests.
 */
class ConnextANOWriter {
 public:
  ConnextANOWriter(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms);
  /**
   * Broadcasts the buffer to all known destinations.
   * Returns the number of successful sends.
   */
  std::size_t broadcast(const MemoryBufferView& buffer);
  
  /**
   * Flushes all pending bursts with timeout.
   * Returns the number of bursts successfully sent.
   */
  int flush(int timeout_ms = 1000);
  
 private:
  std::unique_ptr<ConnextTx> tx_;
  std::chrono::milliseconds poll_interval_ms_;
};

/**
 * DDS Writer for broadcasting payloads.
 * Wraps a DDS payload writer, providing a simple interface to broadcast
 * payloads to all known destinations. Used in DDS tests.
 */
class ConnextDDSWriter {
 public:
  ConnextDDSWriter(const DdsConfig& dds_config, int max_payload_bytes);
  ConnextDDSWriter(const ConnextDDSWriter&) = delete;
  ConnextDDSWriter& operator=(const ConnextDDSWriter&) = delete;
  ~ConnextDDSWriter();
  /**
   * Broadcasts the buffer to all known destinations.
   * Returns the number of successful sends.
   */
  std::size_t broadcast(const MemoryBufferView& buffer);
  /**
   * Returns the underlying DDS writer for test discovery helpers.
   * Used in integration tests with DDSCTestContext_waitForReaders().
   */
  dds::pub::DataWriter<dds::core::BytesTopicType>* get_dds_writer();
 private:
  std::unique_ptr<PayloadWriterInterface> payload_writer_;
};

}  // namespace connext_lib
