#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>
#include "connext_lib/transport/payload_transport.hpp"
#include "connext_lib/resource/resource_managers.hpp"
namespace connext_lib {
class ConnextRx {
 public:
  ConnextRx(std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager,
            std::unique_ptr<PayloadReaderInterface> payload_reader);
  ConnextRx(const ConnextRx&) = delete;
  ConnextRx& operator=(const ConnextRx&) = delete;
  ~ConnextRx();
  bool receive(std::vector<std::uint8_t>& destination,
               std::chrono::milliseconds timeout);
 private:
  std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager_;
  std::unique_ptr<PayloadReaderInterface> payload_reader_;
};
}  // namespace connext_lib
