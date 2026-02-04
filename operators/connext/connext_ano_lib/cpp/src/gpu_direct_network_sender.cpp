/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <connext_ano_lib/gpu_direct_network_sender.h>
#include <connext_ano_lib/internal/cuda_resource_manager.h>
#include <connext_ano_lib/internal/packet_burst_manager.h>
#include <connext_ano_lib/internal/network_utils.h>
#include <advanced_network/common.h>
#include <cuda_runtime.h>
#include <algorithm>

using namespace holoscan::advanced_network;

namespace holoscan::ops {

namespace {
  constexpr uint8_t DEFAULT_IP_TTL = 64;
  constexpr int NUM_CONCURRENT_BATCHES = 4;
}

/**
 * @brief Concrete implementation of GPU Direct network sender
 * 
 * Encapsulates all low-level details:
 * - CUDA resource management (streams, events, GPU buffers)
 * - DPDK packet burst management
 * - Network packet construction (Eth+IP+UDP headers)
 * - Asynchronous transmission pipeline
 * 
 * Thread-safety: Not thread-safe. Use from single thread only.
 */
class GpuDirectNetworkSender : public IGpuDirectNetworkSender {
 public:
  explicit GpuDirectNetworkSender(const SenderConfig& config);
  ~GpuDirectNetworkSender() override = default;
  
  void send(void* gpu_data, size_t size) override;
  bool is_ready() const override;
  size_t max_payload_size() const override;
  TransmissionStats get_stats() const override;
  void reset_stats() override;
  int flush(int timeout_ms) override;
  
 private:
  // Configuration
  int port_id_;
  uint16_t queue_id_;
  uint16_t header_size_;
  uint16_t max_packet_size_;
  SendMode send_mode_;
  
  // CUDA resources
  CudaResourceManager cuda_manager_;
  CudaBuffer gpu_header_;
  
  // DPDK resources
  std::unique_ptr<PacketBurstManager> burst_manager_;
  
  // Statistics
  TransmissionStats stats_;
};

//
// Implementation
//

GpuDirectNetworkSender::GpuDirectNetworkSender(const SenderConfig& config)
    : port_id_(-1),
      queue_id_(config.queue_id),
      header_size_(config.header_size),
      max_packet_size_(config.max_packet_size),
      send_mode_(config.send_mode),
      cuda_manager_(NUM_CONCURRENT_BATCHES),
      gpu_header_(config.header_size) {
  
  // Resolve port ID from interface name
  port_id_ = get_port_id(config.interface_name);
  if (port_id_ == -1) {
    throw NetworkInitException(
        "Interface '" + config.interface_name + "' not found in advanced_network config");
  }
  
  // Parse MAC address
  std::array<uint8_t, 6> mac_bytes;
  try {
    NetworkUtils::parse_mac_address(config.eth_dst_addr, mac_bytes);
  } catch (const std::exception& e) {
    throw InvalidConfigException("Invalid MAC address: " + std::string(e.what()));
  }
  
  // Parse IP addresses (network byte order)
  uint32_t ip_src, ip_dst;
  try {
    ip_src = NetworkUtils::parse_ipv4_address(config.ip_src_addr);
    ip_dst = NetworkUtils::parse_ipv4_address(config.ip_dst_addr);
  } catch (const std::exception& e) {
    throw InvalidConfigException("Invalid IP address: " + std::string(e.what()));
  }
  
  // Build packet header template
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
      config.max_packet_size,
      config.udp_src_port,
      config.udp_dst_port,
      ip_src,
      ip_dst,
      mac_bytes,
      DEFAULT_IP_TTL);
  
  // Copy header template to GPU
  cudaError_t err = cudaMemcpy(gpu_header_.data(), &pkt, sizeof(pkt), cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    throw CudaInitException("Failed to copy packet header to GPU: " + 
                           std::string(cudaGetErrorString(err)));
  }
  
  // Initialize burst manager
  burst_manager_ = std::make_unique<PacketBurstManager>(
      port_id_, queue_id_, header_size_, max_packet_size_);
}

void GpuDirectNetworkSender::send(void* gpu_data, size_t size) {
  // Check readiness (defensive check - caller should pre-check)
  if (!is_ready()) {
    stats_.frames_dropped++;
    throw NotReadyException(cuda_manager_.current_index());
  }
  
  // Truncate if needed
  size_t actual_size = std::min(size, max_payload_size());
  
  // Prepare burst
  constexpr int NUM_PACKETS = 1;
  BurstParams* burst = burst_manager_->prepare_tx_burst(NUM_PACKETS);
  if (burst == nullptr) {
    stats_.frames_dropped++;
    throw NetworkInitException("Failed to prepare TX burst (DPDK pool exhausted)");
  }
  
  // Populate packet data on GPU
  bool success = burst_manager_->populate_tx_packet_data(
      burst, gpu_header_.data(), gpu_data, actual_size, NUM_PACKETS, cuda_manager_);
  
  if (!success) {
    stats_.frames_dropped++;
    throw CudaInitException("Failed to populate packet data on GPU");
  }
  
  // Record event and advance
  cudaEvent_t event = cuda_manager_.get_event();
  cuda_manager_.record_and_advance();
  
  // Enqueue burst
  burst_manager_->enqueue_tx_burst(burst, event);
  
  // Try to send any ready bursts (non-blocking first attempt)
  int sent_count = burst_manager_->send_ready_bursts();
  
  // In IMMEDIATE mode, block until this burst is sent
  // In BATCH mode, just enqueue and return (caller must call flush())
  if (send_mode_ == SendMode::IMMEDIATE && sent_count == 0) {
    // Wait for GPU to finish, then send
    cudaEventSynchronize(event);
    sent_count = burst_manager_->send_ready_bursts();
  }
  
  // Update statistics for any bursts that were sent
  if (sent_count > 0) {
    stats_.packets_sent += sent_count;
    stats_.bytes_transmitted += actual_size * sent_count;
  }
}

bool GpuDirectNetworkSender::is_ready() const {
  return cuda_manager_.is_ready();
}

size_t GpuDirectNetworkSender::max_payload_size() const {
  return max_packet_size_ - header_size_;
}

TransmissionStats GpuDirectNetworkSender::get_stats() const {
  return stats_;
}

void GpuDirectNetworkSender::reset_stats() {
  stats_ = TransmissionStats{};
}

int GpuDirectNetworkSender::flush(int timeout_ms) {
  return burst_manager_->flush_all_bursts(timeout_ms);
}

//
// Factory method
//

std::unique_ptr<IGpuDirectNetworkSender> IGpuDirectNetworkSender::create(
    const SenderConfig& config) {
  return std::make_unique<GpuDirectNetworkSender>(config);
}

}  // namespace holoscan::ops
