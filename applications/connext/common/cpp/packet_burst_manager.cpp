/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <packet_burst_manager.h>
#include <holoscan/holoscan.hpp>

namespace holoscan::ops {

PacketBurstManager::PacketBurstManager(int port_id, uint16_t queue_id,
                                       uint16_t header_size, uint16_t max_packet_size)
    : port_id_(port_id),
      queue_id_(queue_id),
      header_size_(header_size),
      max_packet_size_(max_packet_size) {
  HOLOSCAN_LOG_DEBUG("PacketBurstManager created: port_id={}, queue_id={}, header_size={}, max_packet_size={}",
                     port_id_, queue_id_, header_size_, max_packet_size_);
}

BurstParams* PacketBurstManager::prepare_tx_burst(int num_packets) {
  // Step 1: Create burst parameters structure
  BurstParams* burst = create_tx_burst_params();
  if (burst == nullptr) {
    HOLOSCAN_LOG_ERROR("Failed to create TX burst params");
    return nullptr;
  }
  
  // Step 2: Set header (port, queue, packet count, segments)
  // Single segment (1) for GPU-only mode
  set_header(burst, port_id_, queue_id_, num_packets, 1);
  
  // Step 3: Check if burst is available (throttling check)
  if (!is_tx_burst_available(burst)) {
    HOLOSCAN_LOG_WARN("TX burst not available on port {} queue {}, throttling", 
                      port_id_, queue_id_);
    free_tx_metadata(burst);
    return nullptr;
  }
  
  // Step 4: Get packet burst from pool
  Status ret = get_tx_packet_burst(burst);
  if (ret != Status::SUCCESS) {
    HOLOSCAN_LOG_ERROR("Failed to get TX packet burst: {}", static_cast<int>(ret));
    free_tx_metadata(burst);
    return nullptr;
  }
  
  HOLOSCAN_LOG_DEBUG("Prepared TX burst with {} packets", num_packets);
  return burst;
}

void* PacketBurstManager::get_tx_segment_ptr(BurstParams* burst, int segment, int packet_idx) {
  if (burst == nullptr) {
    HOLOSCAN_LOG_ERROR("Cannot get packet pointer: burst is null");
    return nullptr;
  }
  
  void* ptr = get_segment_packet_ptr(burst, segment, packet_idx);
  if (ptr == nullptr) {
    HOLOSCAN_LOG_ERROR("Failed to get packet pointer for segment {} packet {}", 
                       segment, packet_idx);
  }
  
  return ptr;
}

bool PacketBurstManager::set_tx_packet_length(BurstParams* burst, int packet_idx, uint16_t length) {
  if (burst == nullptr) {
    HOLOSCAN_LOG_ERROR("Cannot set packet length: burst is null");
    return false;
  }
  
  Status ret = set_packet_lengths(burst, packet_idx, {static_cast<int>(length)});
  if (ret != Status::SUCCESS) {
    HOLOSCAN_LOG_ERROR("Failed to set packet length for packet {}: {}", 
                       packet_idx, static_cast<int>(ret));
    return false;
  }
  
  return true;
}

bool PacketBurstManager::populate_tx_packet_data(BurstParams* burst, void* header_template,
                                                 void* payload_data, size_t payload_bytes,
                                                 int num_packets, cudaStream_t stream) {
  if (burst == nullptr) {
    HOLOSCAN_LOG_ERROR("Cannot populate packet data: burst is null");
    return false;
  }
  
  if (header_template == nullptr || payload_data == nullptr) {
    HOLOSCAN_LOG_ERROR("Cannot populate packet data: header or payload pointer is null");
    return false;
  }
  
  for (int pkt_idx = 0; pkt_idx < num_packets; pkt_idx++) {
    // Get GPU packet buffer pointer
    void* gpu_pkt_ptr = get_tx_segment_ptr(burst, 0, pkt_idx);
    if (gpu_pkt_ptr == nullptr) {
      HOLOSCAN_LOG_ERROR("Failed to populate packet {}: could not get packet pointer", pkt_idx);
      free_all_packets_and_burst_tx(burst);
      return false;
    }
    
    // Copy pre-made header from GPU template
    cudaError_t err = cudaMemcpyAsync(gpu_pkt_ptr, header_template, header_size_,
                                       cudaMemcpyDeviceToDevice, stream);
    if (err != cudaSuccess) {
      HOLOSCAN_LOG_ERROR("Failed to copy header for packet {}: {}", 
                         pkt_idx, cudaGetErrorString(err));
      free_all_packets_and_burst_tx(burst);
      return false;
    }
    
    // Copy payload data after header
    err = cudaMemcpyAsync(static_cast<uint8_t*>(gpu_pkt_ptr) + header_size_,
                          payload_data,
                          payload_bytes,
                          cudaMemcpyDeviceToDevice,
                          stream);
    if (err != cudaSuccess) {
      HOLOSCAN_LOG_ERROR("Failed to copy payload for packet {}: {}", 
                         pkt_idx, cudaGetErrorString(err));
      free_all_packets_and_burst_tx(burst);
      return false;
    }
    
    // Set total packet length in metadata
    uint16_t total_len = header_size_ + payload_bytes;
    if (!set_tx_packet_length(burst, pkt_idx, total_len)) {
      HOLOSCAN_LOG_ERROR("Failed to set length for packet {}", pkt_idx);
      free_all_packets_and_burst_tx(burst);
      return false;
    }
  }
  
  HOLOSCAN_LOG_DEBUG("Populated {} packets with header ({} bytes) + payload ({} bytes)",
                     num_packets, header_size_, payload_bytes);
  return true;
}

void PacketBurstManager::enqueue_tx_burst(BurstParams* burst, cudaEvent_t event) {
  if (burst == nullptr) {
    HOLOSCAN_LOG_ERROR("Cannot enqueue: burst is null");
    return;
  }
  
  tx_queue_.push(TxBurst{burst, event});
  HOLOSCAN_LOG_DEBUG("Enqueued TX burst, queue size: {}", tx_queue_.size());
}

int PacketBurstManager::send_ready_bursts() {
  int sent_count = 0;
  
  while (!tx_queue_.empty()) {
    const auto& front = tx_queue_.front();
    
    // Check if CUDA operations completed
    cudaError_t cuda_status = cudaEventQuery(front.event);
    if (cuda_status == cudaSuccess) {
      // CUDA operations done, send burst to NIC
      Status status = send_tx_burst(front.burst);
      if (status != Status::SUCCESS) {
        HOLOSCAN_LOG_ERROR("Failed to send TX burst: {}", static_cast<int>(status));
      } else {
        int num_packets = get_num_packets(front.burst);
        HOLOSCAN_LOG_DEBUG("Sent TX burst with {} packets", num_packets);
        sent_count++;
      }
      
      tx_queue_.pop();
    } else if (cuda_status == cudaErrorNotReady) {
      // Not ready yet, stop checking (maintain FIFO order)
      HOLOSCAN_LOG_DEBUG("TX burst not ready, {} bursts still queued", tx_queue_.size());
      break;
    } else {
      // CUDA error
      HOLOSCAN_LOG_ERROR("CUDA event query error: {}. Removing problematic burst.", cudaGetErrorString(cuda_status));
      tx_queue_.pop();  // Remove problematic burst
    }
  }
  
  return sent_count;
}

}  // namespace holoscan::ops
