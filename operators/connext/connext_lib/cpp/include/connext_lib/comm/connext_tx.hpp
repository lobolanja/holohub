#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include "connext_lib/transport/payload_transport.hpp"
#include "connext_lib/resource/resource_managers.hpp"
namespace connext_lib {
class ConnextTx {
 public:
  ConnextTx(std::unique_ptr<SenderResourcesManagerInterface> sender_manager,
            std::unique_ptr<PayloadWriterInterface> payload_writer,
            std::chrono::milliseconds poll_interval = std::chrono::milliseconds{100});
  ConnextTx(const ConnextTx&) = delete;
  ConnextTx& operator=(const ConnextTx&) = delete;
  ~ConnextTx();
  void setBuffer(const PayloadBufferView& buffer);
  bool sendTo(const std::string& destination_reference);
  std::size_t broadcast();
 private:
  std::unique_ptr<SenderResourcesManagerInterface> sender_manager_;
  std::unique_ptr<PayloadWriterInterface> payload_writer_;
};
}  // namespace connext_lib

