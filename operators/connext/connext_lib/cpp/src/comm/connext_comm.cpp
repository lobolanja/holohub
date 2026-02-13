#include "connext_lib/comm/connext_rx.hpp"
#include "connext_lib/comm/connext_tx.hpp"
#include "connext_lib/resource/resource_managers.hpp"
#include "connext_lib/transport/payload_transport.hpp"
#include "connext_lib/transport/payload_transport_ano.hpp"

#include <holoscan/logger/logger.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace connext_lib {

ConnextRx::ConnextRx(
    std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager,
    std::unique_ptr<PayloadReaderInterface> payload_reader)
    : receiver_manager_(std::move(receiver_manager)),
      payload_reader_(std::move(payload_reader)) {
  if (!receiver_manager_ || !payload_reader_) {
    throw std::invalid_argument("ConnextRx requires valid interfaces");
  }
  // Start a thread to periodically call announce() every 400ms
  announce_thread_ = std::thread([this]() {
    std::lock_guard<std::mutex> lock(announce_mutex_);
    while (!stop_announce_thread_) {
      receiver_manager_->announce();
      std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
  });
}

ConnextRx::~ConnextRx() {
  stop_announce_thread_ = true;
  if (announce_thread_.joinable()) {
    announce_thread_.join();
  }
}

MemoryBufferView ConnextRx::receive(std::chrono::milliseconds timeout) {
  void* data_ptr = nullptr;
  std::size_t size = 0;
  if (!payload_reader_->readNext(data_ptr, size, timeout)) {
    HOLOSCAN_LOG_INFO("ConnextRx: no data received within timeout {} ms", timeout.count());
    return {nullptr, 0, false};
  }
  if (data_ptr == nullptr || size == 0) {
    payload_reader_->freeData(data_ptr);
    return {nullptr, 0, false};
  }
  // Return pointer without copying - caller must call freeBuffer()
  // ANO readers return GPU memory, DDS readers return CPU memory
  // Check if payload_reader_ is an ANOPayloadReader by using dynamic_cast
  bool is_device = (dynamic_cast<ANOPayloadReader*>(payload_reader_.get()) != nullptr);
  return {data_ptr, size, is_device};
}

void ConnextRx::freeBuffer(const MemoryBufferView& buffer) {
  if (buffer.ptr != nullptr) {
    payload_reader_->freeData(buffer.ptr);
  }
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

void ConnextTx::setBuffer(const MemoryBufferView& buffer) {
  payload_writer_->setBuffer(buffer);
}

bool ConnextTx::sendTo(const std::string& destination_reference) {
  return payload_writer_->writeTo(destination_reference);
}

std::size_t ConnextTx::broadcast() {
  std::size_t sent = 0;
  for (const auto& [destination, destination_info] : sender_manager_->destinations()) {
    
    if (payload_writer_->writeTo(destination_info)) {
      ++sent;
    }
  }
  return sent;
}

int ConnextTx::flush(int timeout_ms) {
  return payload_writer_->flush(timeout_ms);
}

}  // namespace connext_lib
