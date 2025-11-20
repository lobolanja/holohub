#include "connext_lib/connext_rx.hpp"
#include "connext_lib/connext_tx.hpp"

#include <stdexcept>

namespace connext_lib {

ConnextRx::ConnextRx(
    std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager,
    std::unique_ptr<PayloadReaderInterface> payload_reader)
    : receiver_manager_(std::move(receiver_manager)),
      payload_reader_(std::move(payload_reader)) {
  if (!receiver_manager_ || !payload_reader_) {
    throw std::invalid_argument("ConnextRx requires valid interfaces");
  }
  if (!receiver_manager_->announce()) {
    throw std::runtime_error("Receiver resources announcement failed");
  }
}

ConnextRx::~ConnextRx() = default;

bool ConnextRx::receive(std::vector<std::uint8_t>& destination,
                        std::chrono::milliseconds timeout) {
  return payload_reader_->readNext(destination, timeout);
}

ConnextTx::ConnextTx(std::unique_ptr<SenderResourcesManagerInterface> sender_manager,
                     std::unique_ptr<PayloadWriterInterface> payload_writer,
                     std::chrono::milliseconds poll_interval)
    : sender_manager_(std::move(sender_manager)),
      payload_writer_(std::move(payload_writer)) {
  if (!sender_manager_ || !payload_writer_) {
    throw std::invalid_argument("ConnextTx requires valid interfaces");
  }
  sender_manager_->startProcessing(poll_interval);
}

ConnextTx::~ConnextTx() {
  if (sender_manager_) {
    sender_manager_->stopProcessing();
  }
}

void ConnextTx::setBuffer(const PayloadBufferView& buffer) {
  payload_writer_->setBuffer(buffer);
}

bool ConnextTx::sendTo(const std::string& destination_reference) {
  return payload_writer_->writeTo(destination_reference);
}

std::size_t ConnextTx::broadcast() {
  std::size_t sent = 0;
  for (const auto& [destination, buffer_ref] : sender_manager_->destinations()) {
    (void)destination;
    if (payload_writer_->writeTo(buffer_ref)) {
      ++sent;
    }
  }
  return sent;
}

}  // namespace connext_lib

