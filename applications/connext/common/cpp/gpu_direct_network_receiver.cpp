/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/gpu_direct_network_receiver.h"
#include "../include/internal/cuda_resource_manager.h"
#include <advanced_network/common.h>
#include <cuda_runtime.h>
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
    
    // Free bursts from previous receive() calls whose CUDA operations completed
    free_completed_bursts();
    
    // Poll all RX queues for new data
    for (int q = 0; q < num_rx_queues_; q++) {
      BurstParams* burst = nullptr;
      Status status = get_rx_burst(&burst, port_id_, q);
      
      if (status != Status::SUCCESS) {
        continue;  // Try next queue
      }
      
      if (burst == nullptr) {
        continue;  // Try next queue
      }
      
      auto burst_size = get_num_packets(burst);
      if (burst_size == 0) {
        free_all_packets_and_burst_rx(burst);
        continue;  // Try next queue
      }
      
      // Got packets! Process first packet
      void* gpu_pkt_ptr = get_packet_ptr(burst, 0);
      uint16_t pkt_len = get_packet_length(burst, 0);
      
      // Validate packet length
      if (pkt_len <= header_size_) {
        free_all_packets_and_burst_rx(burst);
        continue;  // Try next queue
      }
      
      size_t payload_size = pkt_len - header_size_;
      
      // Allocate new GPU buffer for payload
      void* payload_buffer = nullptr;
      cudaError_t err = cudaMalloc(&payload_buffer, payload_size);
      if (err != cudaSuccess) {
        throw CudaInitException(
          std::string("Failed to allocate GPU memory for payload: ") +
          cudaGetErrorString(err));
      }
      
      // Copy payload (skip header) to new buffer
      void* payload_src = static_cast<uint8_t*>(gpu_pkt_ptr) + header_size_;
      cudaStream_t stream = cuda_manager_.get_stream();
      err = cudaMemcpyAsync(payload_buffer, payload_src, payload_size,
                            cudaMemcpyDeviceToDevice, stream);
      if (err != cudaSuccess) {
        cudaFree(payload_buffer);
        throw CudaInitException(
          std::string("Failed to copy payload: ") + cudaGetErrorString(err));
      }
      
      // Record event and advance to next slot
      cudaEvent_t event = cuda_manager_.get_event();
      cuda_manager_.record_and_advance();
      
      // Queue burst for later freeing (after CUDA copy completes)
      batch_queue_.push(RxBatch{burst, event});
      
      // Update statistics
      stats_.packets_received++;
      stats_.bytes_received += payload_size;
      
      // Return the new buffer to caller
      return ReceivedData{payload_buffer, payload_size};
    }
    
    // No data available from any queue
    stats_.empty_polls++;
    return std::nullopt;
  }
  
  void free_received_data(void* gpu_payload) override {
    if (gpu_payload == nullptr) {
      return;
    }
    
    cudaError_t err = cudaFree(gpu_payload);
    if (err != cudaSuccess) {
      throw CudaInitException(
        std::string("Failed to free GPU payload: ") + cudaGetErrorString(err));
    }
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
  // Configuration
  int port_id_;
  int num_rx_queues_;
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
