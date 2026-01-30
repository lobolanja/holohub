#include "connext_lib/transport/payload_transport_ano.hpp"
#include "connext_lib/transport/sender_factory.hpp"
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

ANOPayloadWriter::ANOPayloadWriter(dds::domain::DomainParticipant dp,
                                   const PayloadWriterOptions& opts,
                                   const AnoConfig& ano_config,
                                   std::shared_ptr<ISenderFactory> sender_factory)
    : ano_config_(ano_config), max_payload_bytes_(opts.max_payload_bytes), dp_(dp), sender_factory_(sender_factory) {
  if (!sender_factory_) {
    // Production SenderFactory will be used; real implementation lives elsewhere.
    sender_factory_ = std::make_shared<SenderFactory>();
  }
}

ANOPayloadWriter::~ANOPayloadWriter() {
  std::lock_guard<std::mutex> g(writer_mutex_);
  // TODO: free staged_gpu_ptr_ if allocated (cudaFree)

}

void ANOPayloadWriter::setBuffer(const PayloadBufferView& buffer) {
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
  
  // Device pointer path: caller provided CUDA pointer, store reference only.
  staged_gpu_ptr_ = buffer.device_ptr;
  staged_size_ = buffer.size_bytes;
  HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: staged device buffer {} bytes", staged_size_);
  
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
    }
    
  } catch (const std::exception& e) {
    HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: send exception: {}", e.what());
    return false;
  } catch (...) {
    HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: unknown send exception");
    return false;
  }

  return true;
}

void ANOPayloadWriter::registerDestination(const std::string& destination_reference, const SenderDestination& dest) {
  std::lock_guard<std::mutex> g(writer_mutex_);
  // store the discovered destination metadata keyed by the canonical destination_reference
  dest_map_[destination_reference] = dest;
}

void ANOPayloadWriter::unregisterDestination(const std::string& destination_reference) {
  std::lock_guard<std::mutex> g(writer_mutex_);
  auto dit = dest_map_.find(destination_reference);
  if (dit != dest_map_.end()) {
    sender_cache_.erase(destination_reference);
    dest_map_.erase(dit);
  }
}
std::shared_ptr<holoscan::ops::IGpuDirectNetworkSender> ANOPayloadWriter::getOrCreateSenderFor(const std::string& destination_reference) {
  // assumes caller does not hold mutex
  std::lock_guard<std::mutex> g(writer_mutex_);
  // Use the canonical destination_reference (same as registration key)
  const std::string& key = destination_reference;

  // If we already have a cached sender for this key, return it.
  auto it = sender_cache_.find(key);
  if (it != sender_cache_.end()) return it->second;

  // If the discovery map contains structured info for this key, prefer it.
  auto dit = dest_map_.find(destination_reference);
  std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> up;
  if (dit != dest_map_.end()) {
    try {
      up = sender_factory_->create_sender(dit->second);
    } catch (const std::exception& e) {
      HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: sender factory failed for structured dest {}: {}", key, e.what());
      return nullptr;
    }
  }

  if (!up) return nullptr;
  auto sp = std::shared_ptr<holoscan::ops::IGpuDirectNetworkSender>(std::move(up));
  sender_cache_.emplace(key, sp);
  HOLOSCAN_LOG_DEBUG("ANOPayloadWriter: created sender for {}", key);
  return sp;
}

std::unique_ptr<PayloadWriterInterface> MakeANOPayloadWriter(dds::domain::DomainParticipant dp,
                                                             const PayloadWriterOptions& opts,
                                                             const AnoConfig& ano_config,
                                                             std::shared_ptr<ISenderFactory> sender_factory) {
  return std::make_unique<ANOPayloadWriter>(dp, opts, ano_config, sender_factory);
}

ANOPayloadReader::ANOPayloadReader(dds::domain::DomainParticipant dp,
                                   const PayloadReaderOptions& opts,
                                   const AnoConfig& ano_config,
                                   std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> receiver)
    : ano_config_(ano_config), dp_(dp) {
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
    cfg.header_size = 64; // conservative header strip size
    cfg.max_packet_size = static_cast<uint16_t>(ano_config_.max_payload_bytes() + cfg.header_size);
    cfg.gpu_device = gcfg.gpu_device_id();
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
        auto r = maybe.value();
        data_ptr = r.gpu_payload;
        size = r.payload_bytes;
        return true;
      }
    } catch (const std::exception& e) {
      HOLOSCAN_LOG_DEBUG("ANOPayloadReader::readNext exception: {}", e.what());
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
    dds::domain::DomainParticipant dp,
    const PayloadReaderOptions& opts,
    const AnoConfig& ano_config) {
  return std::make_unique<ANOPayloadReader>(dp, opts, ano_config);
}

std::unique_ptr<ANOPayloadReader> MakeANOPayloadReader(dds::domain::DomainParticipant& participant,
                                                      const PayloadReaderOptions& opts,
                                                      const AnoConfig& ano_config,
                                                      std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> receiver) {
  return std::make_unique<ANOPayloadReader>(participant, opts, ano_config, std::move(receiver));
}

}  // namespace connext_lib
