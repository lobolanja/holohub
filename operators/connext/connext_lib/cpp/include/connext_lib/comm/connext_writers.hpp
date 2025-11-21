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
   * Broadcasts the staged payload to all known destinations.
   * Returns the number of successful sends.
   */
  std::size_t broadcast(const PayloadBufferView& buffer);
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
  explicit ConnextDDSWriter(std::unique_ptr<PayloadWriterInterface> payload_writer);
  ConnextDDSWriter(const ConnextDDSWriter&) = delete;
  ConnextDDSWriter& operator=(const ConnextDDSWriter&) = delete;
  ~ConnextDDSWriter();
  /**
   * Broadcasts the staged payload to all known destinations.
   * Returns the number of successful sends.
   */
  std::size_t broadcast(const PayloadBufferView& buffer);
 private:
  std::unique_ptr<PayloadWriterInterface> payload_writer_;
};

}  // namespace connext_lib
