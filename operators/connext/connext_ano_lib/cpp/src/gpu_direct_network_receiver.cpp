/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <connext_ano_lib/gpu_direct_network_receiver.h>
#include <connext_ano_lib/internal/cuda_resource_manager.h>
#include <advanced_network/common.h>
#include <cuda_runtime.h>
#include <arpa/inet.h>
#include <queue>

using namespace holoscan::advanced_network;

namespace holoscan::ops {

namespace {
  constexpr int NUM_CONCURRENT_SLOTS = 4;
}

/**
 * @brief Concrete implementation of GPU Direct network receiver
 * 
 * Encapsulates all low-level details:
 * - CUDA resource management (streams, events, GPU buffers)
 * - DPDK burst management and multi-queue polling
 * - Packet header parsing and payload extraction
 * - Async burst cleanup (deferred freeing after CUDA completion)
 * 
 * Internal state:
 * - port_id_: DPDK port ID resolved from interface name
 * - num_rx_queues_: Number of RX queues on port
 * - header_size_, max_packet_size_: From config
 * - cuda_manager_: Manages CUDA streams/events for async operations
 * - batch_queue_: Tracks in-flight bursts awaiting CUDA completion
 * - stats_: Cumulative statistics
 */
class GpuDirectNetworkReceiver : public IGpuDirectNetworkReceiver {
 public:
  explicit GpuDirectNetworkReceiver(const ReceiverConfig& config)
    : header_size_(config.header_size),
      max_packet_size_(config.max_packet_size),
      gpu_device_(config.gpu_device),
      queue_id_(config.queue_id),
      cuda_manager_(NUM_CONCURRENT_SLOTS) {
    
    // Set GPU device
    cudaError_t err = cudaSetDevice(gpu_device_);
    if (err != cudaSuccess) {
      throw CudaInitException(
        std::string("Failed to set GPU device ") + std::to_string(gpu_device_) +
        ": " + cudaGetErrorString(err));
    }
    
    // Resolve port ID from interface name
    port_id_ = get_port_id(config.interface_name);
    if (port_id_ == -1) {
      throw NetworkInitException(
        "Failed to resolve interface '" + config.interface_name + "' to port ID");
    }
    
    // Query number of RX queues
    num_rx_queues_ = get_num_rx_queues(port_id_);
    if (num_rx_queues_ <= 0) {
      throw NetworkInitException(
        "Port " + std::to_string(port_id_) + " has no RX queues configured");
    }
  }
  
  ~GpuDirectNetworkReceiver() override {
    // Free any pending bursts
    while (!batch_queue_.empty()) {
      free_all_packets_and_burst_rx(batch_queue_.front().burst);
      batch_queue_.pop();
    }
  }
  
  std::optional<ReceivedData> receive() override {
    stats_.polls_attempted++;
    free_completed_bursts();
    
    auto burst = poll_for_burst();
    if (!burst.has_value()) {
      stats_.empty_polls++;
      return std::nullopt;
    }
    
    auto packet_info = extract_payload_from_packet(burst.value());
    if (!packet_info.has_value()) {
      free_all_packets_and_burst_rx(burst.value());
      stats_.empty_polls++;
      return std::nullopt;
    }

    void* payload_buffer = cuda_manager_.allocate_buffer(packet_info->payload_size);
    try {
      void* payload_src = static_cast<uint8_t*>(packet_info->gpu_pkt_ptr) + header_size_;
      cuda_manager_.async_copy_device_to_device(payload_buffer, payload_src, packet_info->payload_size);
    } catch (...) {
      cuda_manager_.free_buffer(payload_buffer);
      throw;
    }
    
    enqueue_burst_for_cleanup(burst.value());
    
    stats_.packets_received++;
    stats_.bytes_received += packet_info->payload_size;
    
    return ReceivedData{payload_buffer, packet_info->payload_size};
  }
  
  void free_received_data(void* gpu_payload) override {
    cuda_manager_.free_buffer(gpu_payload);
  }
  
  size_t max_payload_size() const override {
    return max_packet_size_ - header_size_;
  }
  
  ReceptionStats get_stats() const override {
    return stats_;
  }
  
  void reset_stats() override {
    stats_ = ReceptionStats{};
  }
  
 private:
  // Helper struct to return packet information from validation
  struct PacketInfo {
    void* gpu_pkt_ptr;
    size_t payload_size;
  };
  
  // Configuration
  int port_id_;
  uint16_t queue_id_;       ///< RX queue ID to poll from
  int num_rx_queues_;       ///< Number of RX queues on port (for validation)
  uint16_t header_size_;
  uint16_t max_packet_size_;
  int gpu_device_;
  
  // CUDA resource management
  CudaResourceManager cuda_manager_;
  
  // Burst queue tracking
  struct RxBatch {
    BurstParams* burst;
    cudaEvent_t evt;
  };
  std::queue<RxBatch> batch_queue_;
  
  // Statistics
  ReceptionStats stats_;
  
  /**
   * @brief Poll the configured RX queue for a valid burst
   * 
   * Polls only the specific queue_id configured for this receiver.
   * Automatically frees empty bursts.
   * 
   * @return BurstParams* if valid burst found, std::nullopt otherwise
   */
  std::optional<BurstParams*> poll_for_burst() {
    BurstParams* burst = nullptr;
    Status status = get_rx_burst(&burst, port_id_, queue_id_);
    
    if (status != Status::SUCCESS || burst == nullptr) {
      return std::nullopt;
    }
    
    auto burst_size = get_num_packets(burst);
    if (burst_size == 0) {
      free_all_packets_and_burst_rx(burst);
      return std::nullopt;
    }

    HOLOSCAN_LOG_DEBUG("Received burst with {} packets from port {} queue {}", 
                     burst_size, port_id_, queue_id_);
      
    return burst;
  }
  
  /**
   * @brief Validate packet and extract payload information
   * 
   * Checks packet length is sufficient to contain headers and payload.
   * 
   * @param burst DPDK burst containing packet
   * @return PacketInfo if valid, std::nullopt if packet too small
   */
  std::optional<PacketInfo> extract_payload_from_packet(BurstParams* burst) {
    void* gpu_pkt_ptr = get_packet_ptr(burst, 0);
    uint16_t pkt_len = get_packet_length(burst, 0);
    HOLOSCAN_LOG_DEBUG("Extracted packet length: {} bytes", pkt_len);
    
    if (pkt_len <= header_size_) {
      return std::nullopt;
    }
    
    size_t payload_size = pkt_len - header_size_;
    return PacketInfo{gpu_pkt_ptr, payload_size};
  }
  
  /**
   * @brief Record CUDA event and enqueue burst for deferred cleanup
   * 
   * Associates burst with CUDA event for async tracking. Burst will be
   * freed later by free_completed_bursts() once CUDA operations complete.
   * 
   * @param burst DPDK burst to enqueue
   */
  void enqueue_burst_for_cleanup(BurstParams* burst) {
    cudaEvent_t event = cuda_manager_.get_event();
    cuda_manager_.record_and_advance();
    batch_queue_.push(RxBatch{burst, event});
  }
  
  /**
   * @brief Free DPDK bursts whose CUDA copy operations have completed
   * 
   * Checks queue of pending bursts and frees those whose associated
   * CUDA event has completed. Called at start of each receive().
   */
  void free_completed_bursts() {
    while (!batch_queue_.empty()) {
      const auto& batch = batch_queue_.front();
      if (cudaEventQuery(batch.evt) == cudaSuccess) {
        free_all_packets_and_burst_rx(batch.burst);
        batch_queue_.pop();
      } else {
        break;  // Queue is ordered, so stop at first incomplete
      }
    }
  }
};

// Factory method implementation
std::unique_ptr<IGpuDirectNetworkReceiver> 
IGpuDirectNetworkReceiver::create(const ReceiverConfig& config) {
  // Validate configuration
  config.validate();
  
  // Create concrete implementation
  return std::make_unique<GpuDirectNetworkReceiver>(config);
}

}  // namespace holoscan::ops
