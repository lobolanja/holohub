#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "connext_lib/payload_transport.hpp"
#include "connext_lib/resource_managers.hpp"

namespace connext_lib {

/** High-level receiver helper that wires resource announcements to the
 *  payload transport reader. */
class ConnextRx {
 public:
  ConnextRx(std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager,
            std::unique_ptr<PayloadReaderInterface> payload_reader);

  ConnextRx(const ConnextRx&) = delete;
  ConnextRx& operator=(const ConnextRx&) = delete;

  ~ConnextRx();

  /** Blocks until a payload arrives or the timeout expires. */
  bool receive(std::vector<std::uint8_t>& destination,
               std::chrono::milliseconds timeout);

 private:
  std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager_;
  std::unique_ptr<PayloadReaderInterface> payload_reader_;
};

}  // namespace connext_lib

