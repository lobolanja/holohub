#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "connext_lib/payload_transport.hpp"
#include "connext_lib/resource_managers.hpp"

namespace connext_lib {

/** High-level transmitter that publishes buffers to every registered receiver. */
class ConnextTx {
 public:
  ConnextTx(std::unique_ptr<SenderResourcesManagerInterface> sender_manager,
            std::unique_ptr<PayloadWriterInterface> payload_writer,
            std::chrono::milliseconds poll_interval = std::chrono::milliseconds{100});

  ConnextTx(const ConnextTx&) = delete;
  ConnextTx& operator=(const ConnextTx&) = delete;

  ~ConnextTx();

  /** Stages the payload so it can be broadcast or sent to a single destination. */
  void setBuffer(const PayloadBufferView& buffer);

  /** Sends the staged payload to a specific destination. */
  bool sendTo(const std::string& destination_reference);

  /** Sends the staged payload to every known destination. */
  std::size_t broadcast();

 private:
  std::unique_ptr<SenderResourcesManagerInterface> sender_manager_;
  std::unique_ptr<PayloadWriterInterface> payload_writer_;
};

}  // namespace connext_lib

