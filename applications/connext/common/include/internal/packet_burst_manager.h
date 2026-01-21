/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <advanced_network/common.h>
#include <cuda_runtime.h>
#include <queue>

using namespace holoscan::advanced_network;

namespace holoscan::ops {

/**
 * @brief Manages Advanced Network TX burst lifecycle with CUDA synchronization
 * 
 * Encapsulates the low-level details of allocating, populating, and transmitting
 * packet bursts using the Advanced Network library. Integrates CUDA event-based
 * synchronization for asynchronous GPU operations.
 * 
 * Responsibilities:
 * - TX burst allocation and validation (create, check availability, get packets)
 * - Packet pointer retrieval for GPU memory access
 * - CUDA-synchronized burst queuing (enqueue with events)
 * - Completion-based burst transmission (query events, send to NIC)
 * 
 * This class separates Advanced Network API concerns from Holoscan operator logic,
 * enabling code reuse and clearer abstraction boundaries.
 * 
 * Thread-safety: Not thread-safe. Each instance should be used by a single thread only.
 */
class PacketBurstManager {
 public:
  /**
   * @brief Construct a PacketBurstManager for TX operations
   * 
   * @param port_id DPDK port ID for network interface
   * @param queue_id TX queue ID for packet transmission
   * @param header_size Size of packet headers (Eth+IP+UDP) in bytes
   * @param max_packet_size Maximum packet size including headers
   */
  PacketBurstManager(int port_id, uint16_t queue_id, 
                     uint16_t header_size, uint16_t max_packet_size);
  
  ~PacketBurstManager() = default;
  
  //
  // TX Burst Operations
  //
  
  /**
   * @brief Prepare a TX burst for transmission
   * 
   * Allocates burst metadata and packet buffers from the DPDK pool.
   * Performs the following steps:
   * 1. Creates burst parameters structure
   * 2. Sets header (port, queue, packet count, segments)
   * 3. Checks if burst is available (throttling)
   * 4. Gets packet burst from pool
   * 
   * @param num_packets Number of packets to allocate in burst
   * @return BurstParams* on success, nullptr on failure
   */
  BurstParams* prepare_tx_burst(int num_packets);
  
  /**
   * @brief Populate packet data (header + payload) on GPU
   * 
   * Copies pre-made header and payload data into packet buffers using
   * GPU-to-GPU async copy operations on the provided CUDA stream.
   * 
   * Steps for each packet:
   * 1. Get GPU packet pointer from burst
   * 2. Copy pre-made header from GPU template (GPU-to-GPU)
   * 3. Copy payload data after header (GPU-to-GPU)
   * 4. Set total packet length in burst metadata
   * 
   * @param burst Packet burst to populate
   * @param header_template GPU pointer to pre-made header template
   * @param payload_data GPU pointer to payload data
   * @param payload_bytes Size of payload in bytes
   * @param num_packets Number of packets to populate
   * @param stream CUDA stream for async operations
   * @return true if all packets populated successfully, false on error
   */
  bool populate_tx_packet_data(BurstParams* burst, void* header_template,
                               void* payload_data, size_t payload_bytes,
                               int num_packets, cudaStream_t stream);
  
  /**
   * @brief Enqueue burst for asynchronous transmission
   * 
   * Records a CUDA event on the provided stream and queues the burst
   * for transmission after CUDA operations complete. The burst can
   * be sent to the NIC once the event signals completion.
   * 
   * @param burst Burst to enqueue for transmission
   * @param event CUDA event to track completion of async operations
   */
  void enqueue_tx_burst(BurstParams* burst, cudaEvent_t event);
  
  /**
   * @brief Send ready TX bursts to NIC
   * 
   * Checks queued bursts for CUDA completion and sends ready bursts
   * to the network interface. Maintains FIFO ordering - stops at the
   * first burst that is not ready.
   * 
   * @return Number of bursts sent in this call
   */
  int send_ready_bursts();
  
  //
  // Utility Methods
  //
  
  /**
   * @brief Get header size
   * @return Size of packet headers in bytes
   */
  uint16_t header_size() const { return header_size_; }
  
  /**
   * @brief Get max packet size
   * @return Maximum packet size including headers
   */
  uint16_t max_packet_size() const { return max_packet_size_; }
  
  /**
   * @brief Get max payload size
   * @return Maximum payload size (max_packet_size - header_size)
   */
  uint16_t max_payload_size() const { return max_packet_size_ - header_size_; }
  
 private:
  //
  // Configuration
  //
  int port_id_;                ///< DPDK port ID
  uint16_t queue_id_;          ///< TX queue ID
  uint16_t header_size_;       ///< Size of packet headers (Eth+IP+UDP)
  uint16_t max_packet_size_;   ///< Maximum packet size including headers
  
  //
  // TX Tracking
  //
  
  /**
   * @brief TX burst tracking structure
   * 
   * Tracks a burst that has been enqueued for transmission but may
   * still have in-flight CUDA operations. The event allows checking
   * completion status before sending to NIC.
   */
  struct TxBurst {
    BurstParams* burst;   ///< Pointer to the packet burst
    cudaEvent_t event;    ///< CUDA event to query completion
  };
  
  std::queue<TxBurst> tx_queue_;  ///< Queue of pending transmissions
  
  //
  // Internal Helper Methods
  //
  
  /**
   * @brief Get GPU packet pointer for writing data
   * 
   * Retrieves the GPU-accessible packet buffer pointer for a specific
   * packet and segment within a burst. Used for GPU-to-GPU copies.
   * 
   * @param burst Burst structure containing packets
   * @param segment Segment index (0 for single-segment packets)
   * @param packet_idx Index of packet within burst
   * @return void* GPU-accessible packet buffer pointer, nullptr on error
   */
  void* get_tx_segment_ptr(BurstParams* burst, int segment, int packet_idx);
  
  /**
   * @brief Set packet length in burst metadata
   * 
   * Updates the packet length metadata for a specific packet.
   * Must be called after populating packet data.
   * 
   * @param burst Burst structure containing packets
   * @param packet_idx Index of packet within burst
   * @param length Total packet length (header + payload) in bytes
   * @return true on success, false on error
   */
  bool set_tx_packet_length(BurstParams* burst, int packet_idx, uint16_t length);
  
  /**
   * @brief Copy header to packet buffer on GPU
   * 
   * Performs async GPU-to-GPU copy of pre-made header template.
   * 
   * @param gpu_pkt_ptr Destination packet buffer on GPU
   * @param header_template Source header template on GPU
   * @param stream CUDA stream for async operation
   * @throws std::runtime_error on CUDA copy failure
   */
  void copy_packet_header(void* gpu_pkt_ptr, void* header_template, cudaStream_t stream);
  
  /**
   * @brief Copy payload to packet buffer on GPU (after header)
   * 
   * Performs async GPU-to-GPU copy of payload data after header.
   * 
   * @param gpu_pkt_ptr Destination packet buffer on GPU
   * @param payload_data Source payload data on GPU
   * @param payload_bytes Size of payload in bytes
   * @param stream CUDA stream for async operation
   * @throws std::runtime_error on CUDA copy failure
   */
  void copy_packet_payload(void* gpu_pkt_ptr, void* payload_data, 
                          size_t payload_bytes, cudaStream_t stream);
  
  /**
   * @brief Configure packet metadata (length)
   * 
   * Sets total packet length in burst metadata.
   * 
   * @param burst Burst structure containing packets
   * @param packet_idx Index of packet within burst
   * @param payload_bytes Size of payload in bytes
   * @throws std::runtime_error on metadata update failure
   */
  void configure_packet_metadata(BurstParams* burst, int packet_idx, size_t payload_bytes);
};

}  // namespace holoscan::ops
