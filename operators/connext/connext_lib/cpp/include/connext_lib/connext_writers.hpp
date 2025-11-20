#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "connext_lib/config.hpp"
#include "connext_lib/connext_tx.hpp"
#include "connext_lib/payload_transport.hpp"

namespace connext_lib {

class ConnextANOWriter {
 public:
  ConnextANOWriter(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms);

 /** Broadcasts the staged payload to all known destinations. */
  std::size_t broadcast(const PayloadBufferView& buffer);

 private:
  std::unique_ptr<ConnextTx> tx_;
  std::chrono::milliseconds poll_interval_ms_;
};

class ConnextDDSWriter {
 public:
  explicit ConnextDDSWriter(std::unique_ptr<PayloadWriterInterface> payload_writer);

  ConnextDDSWriter(const ConnextDDSWriter&) = delete;
  ConnextDDSWriter& operator=(const ConnextDDSWriter&) = delete;
  ~ConnextDDSWriter();

  /** Broadcasts the staged payload to all known destinations. */
  std::size_t broadcast(const PayloadBufferView& buffer);

 private:
  std::unique_ptr<PayloadWriterInterface> payload_writer_;
};

}  // namespace connext_lib
