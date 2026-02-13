#include "connext_lib/transport/payload_transport_ano.hpp"
#include "connext_lib/transport/sender_factory.hpp"
#include "connext_lib/transport/sender_info.hpp"
#include "connext_ano_lib/gpu_direct_network_sender.h"
#include "connext_ano_lib/gpu_direct_network_receiver.h"
#include "connext_ano_lib/receiver_config.h"
#include "connext_lib/config/config.hpp"
#include <holoscan/logger/logger.hpp>
#include <stdexcept>
#include <cstring>
#include <sstream>
#include <iostream>

#include <chrono>
#include <thread>

namespace connext_lib {

ANOPayloadWriter::ANOPayloadWriter(const AnoConfig& ano_config,
                   std::shared_ptr<ISenderFactory> sender_factory)
  : ano_config_(ano_config), max_payload_bytes_(ano_config.max_payload_bytes()), sender_factory_(sender_factory) {
  if (!sender_factory_) {
    // Production SenderFactory with config as single source of truth
    sender_factory_ = std::make_shared<SenderFactory>(ano_config_.ano_network_config());
  }
}

ANOPayloadWriter::~ANOPayloadWriter() {
  // staged_gpu_ptr_ is always a borrowed pointer from setBuffer() - caller owns it
  // No cleanup needed
}

void ANOPayloadWriter::setBuffer(const MemoryBufferView& buffer) {
  if (buffer.size_bytes == 0) {
    std::lock_guard<std::mutex> g(writer_mutex_);
    staged_size_ = 0;
    staged_gpu_ptr_ = nullptr;
    return;
  }

  if (max_payload_bytes_ > 0 && buffer.size_bytes > max_payload_bytes_) {
    throw std::length_error("ANO payload exceeds configured maximum size");
  }

  std::lock_guard<std::mutex> g(writer_mutex_);
  
  // Use the pointer directly (GPU or CPU) - caller owns this memory
  staged_gpu_ptr_ = buffer.ptr;
  staged_size_ = buffer.size_bytes;
  HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: staged buffer {} bytes", staged_size_);
  
}

bool ANOPayloadWriter::writeTo(const std::string& destination_reference) {
  // Non-blocking send using the canonical destination_reference key.
  std::shared_ptr<holoscan::ops::IGpuDirectNetworkSender> sender;
  {
    std::lock_guard<std::mutex> g(writer_mutex_);
    if (staged_size_ == 0) return false;
  }

  sender = getOrCreateSenderFor(destination_reference);
  if (!sender) {
    HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: no sender for destination {}", destination_reference);
    return false;
  }

  try {
    if (!sender->is_ready()) {
      HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: sender not ready for destination {}", destination_reference);
      return false;
    }

    {
      std::lock_guard<std::mutex> g(writer_mutex_);
      sender->send(staged_gpu_ptr_, staged_size_);
      //sender->flush(100);  // flush with 100ms timeout
      HOLOSCAN_LOG_INFO("ANOPayloadWriter: sent {} bytes to destination {}", staged_size_, destination_reference);
    }
    
  } catch (const std::exception& e) {
    HOLOSCAN_LOG_WARN("ANOPayloadWriter: send exception: {}", e.what());
    return false;
  } catch (...) {
    HOLOSCAN_LOG_WARN("ANOPayloadWriter: unknown send exception");
    return false;
  }

  return true;
}

int ANOPayloadWriter::flush(int timeout_ms) {
  int total_flushed = 0;
  std::lock_guard<std::mutex> g(writer_mutex_);
  
  // Flush all active senders
  for (const auto& [key, sender] : sender_cache_) {
    if (sender && sender->is_ready()) {
      int flushed = sender->flush(timeout_ms);
      total_flushed += flushed;
    }
  }
  
  return total_flushed;
}

std::shared_ptr<holoscan::ops::IGpuDirectNetworkSender> ANOPayloadWriter::getOrCreateSenderFor(const std::string& destination_reference) {
  // assumes caller does not hold mutex
  std::lock_guard<std::mutex> g(writer_mutex_);
  // Use the canonical destination_reference (same as registration key)
  const std::string& key = destination_reference;

  // If we already have a cached sender for this key, return it.
  auto it = sender_cache_.find(key);
  if (it != sender_cache_.end()) return it->second;

  std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> up;

  // Try to parse the destination_reference as a canonical string
  try {
    DestinationInfo dest = DestinationInfo::fromString(destination_reference);
    up = sender_factory_->create_sender(dest);
    HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: created sender from parsed destination {}", key);
  } catch (const std::exception& e) {
    HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: failed to parse or create sender for {}: {}", key, e.what());
    return nullptr;
  }

  if (!up) return nullptr;
  auto sp = std::shared_ptr<holoscan::ops::IGpuDirectNetworkSender>(std::move(up));
  sender_cache_.emplace(key, sp);
  HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: created sender for {}", key);
  return sp;
}

std::unique_ptr<PayloadWriterInterface> MakeANOPayloadWriter(
    const AnoConfig& ano_config,
    std::shared_ptr<ISenderFactory> sender_factory) {
  return std::make_unique<ANOPayloadWriter>(ano_config, sender_factory);
}

ANOPayloadReader::ANOPayloadReader(const AnoConfig& ano_config,
                   std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> receiver)
  : ano_config_(ano_config) {
  // If a receiver was injected (tests), use it; otherwise create from config
  if (receiver) {
    receiver_ = std::move(receiver);
    return;
  }

  // Build a ReceiverConfig from AnoConfig's GPU receiver settings
  try {
    holoscan::ops::ReceiverConfig cfg;
    const auto& gcfg = ano_config_.ano_network_config();
    cfg.interface_name = gcfg.network_interface();
    cfg.header_size = gcfg.header_size();
    cfg.max_packet_size = gcfg.max_packet_size();
    cfg.gpu_device = gcfg.gpu_device_id();
    cfg.queue_id = gcfg.queue_id();
    cfg.validate();
    receiver_ = holoscan::ops::IGpuDirectNetworkReceiver::create(cfg);
  } catch (const std::exception& e) {
    HOLOSCAN_LOG_DEBUG("ANOPayloadReader: failed to create receiver: {}", e.what());
    receiver_.reset();
  }
}

ANOPayloadReader::~ANOPayloadReader() {
  // Receiver unique_ptr will be freed automatically
}

bool ANOPayloadReader::readNext(void*& data_ptr, std::size_t& size,
                                std::chrono::milliseconds timeout) {
  if (!receiver_) return false;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    try {
      auto maybe = receiver_->receive();
      if (maybe.has_value()) {
        std::string debug_ip_port_mac = this->ano_config_.ano_network_config().fast_ip() + ":" +
                                        std::to_string(this->ano_config_.ano_network_config().fast_port()) + " " +
                                        this->ano_config_.ano_network_config().fast_mac_address();
         HOLOSCAN_LOG_DEBUG("ANOPayloadReader: {} received {} bytes", debug_ip_port_mac, maybe->payload_bytes);
        auto r = maybe.value();
        data_ptr = r.gpu_payload;
        size = r.payload_bytes;
        return true;
      }
    } catch (const std::exception& e) {
      HOLOSCAN_LOG_WARN("ANOPayloadReader::readNext exception: {}", e.what());
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

void ANOPayloadReader::freeData(void* data_ptr) {
  if (!receiver_ || !data_ptr) return;
  try {
    receiver_->free_received_data(data_ptr);
  } catch (const std::exception& e) {
    HOLOSCAN_LOG_DEBUG("ANOPayloadReader::freeData exception: {}", e.what());
  }
}

std::unique_ptr<PayloadReaderInterface> MakeANOPayloadReader(
    const AnoConfig& ano_config) {
  return std::make_unique<ANOPayloadReader>(ano_config);
}

std::unique_ptr<ANOPayloadReader> MakeANOPayloadReader(
    const AnoConfig& ano_config,
    std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> receiver) {
  return std::make_unique<ANOPayloadReader>(ano_config, std::move(receiver));
}

}  // namespace connext_lib
