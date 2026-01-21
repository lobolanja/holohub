/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "sender_config.h"

namespace holoscan::ops {

/**
 * @brief Transmission statistics
 * 
 * Cumulative counters reset only by reset_stats().
 */
struct TransmissionStats {
  uint64_t packets_sent = 0;      ///< Total packets transmitted to NIC
  uint64_t bytes_transmitted = 0; ///< Total payload bytes transmitted
  uint64_t frames_dropped = 0;    ///< Frames skipped due to not-ready state
};

/**
 * @brief Interface for GPU Direct network transmission
 * 
 * Provides high-level API for transmitting GPU memory buffers over network
 * using GPUDirect and DPDK. Hides all low-level CUDA, DPDK, and packet
 * construction details from callers.
 * 
 * Features:
 * - Zero-copy GPU-to-NIC transmission via GPUDirect
 * - Asynchronous CUDA operations with event-based flow control
 * - Automatic packet header construction (Eth+IP+UDP)
 * - Payload truncation if size exceeds max_payload_size()
 * - Cumulative statistics tracking
 * 
 * Configuration:
 * - GPU-only mode: No header-data split, entire packet on GPU
 * - Hardcoded IP TTL: 64
 * - Hardcoded concurrent batches: 4
 * 
 * Thread-safety: NOT thread-safe. Use from single thread only.
 * 
 * Example usage:
 * ```cpp
 * // Initialization
 * SenderConfig config;
 * config.interface_name = "tx_port";
 * config.queue_id = 0;
 * config.ip_src_addr = "192.168.10.10";
 * config.ip_dst_addr = "192.168.10.11";
 * config.eth_dst_addr = "3c:6d:66:11:91:56";
 * config.udp_src_port = 4096;
 * config.udp_dst_port = 4096;
 * config.header_size = 64;
 * config.max_packet_size = 1064;
 * config.validate();  // Throws on invalid config
 * 
 * auto sender = IGpuDirectNetworkSender::create(config);
 * 
 * // Runtime transmission
 * if (sender->is_ready()) {
 *   void* gpu_data = ...;  // CUDA device pointer
 *   size_t bytes = ...;
 *   sender->send(gpu_data, bytes);  // May throw NotReadyException
 * } else {
 *   // Skip frame, previous batch in flight
 * }
 * ```
 */
class IGpuDirectNetworkSender {
 public:
  virtual ~IGpuDirectNetworkSender() = default;
  
  /**
   * @brief Send GPU buffer to network
   * 
   * Transmits data from GPU memory to network via GPUDirect.
   * Silently truncates payload if size > max_payload_size().
   * 
   * Precondition: is_ready() should return true to avoid NotReadyException.
   * Caller is responsible for checking readiness and deciding whether to
   * skip frame or wait.
   * 
   * @param gpu_data Pointer to GPU memory buffer
   * @param size Size in bytes to transmit
   * @throws NotReadyException if is_ready() returns false (previous batch in flight)
   * @throws CudaInitException if CUDA operations fail
   * 
   * Side effects:
   * - Updates packets_sent and bytes_transmitted statistics
   * - May increment frames_dropped if not ready (only in exception path)
   */
  virtual void send(void* gpu_data, size_t size) = 0;
  
  /**
   * @brief Check if sender is ready for transmission
   * 
   * Returns true if previous CUDA batch has completed and sender can
   * accept new data. Always call this before send() to avoid exceptions.
   * 
   * @return true if ready for send(), false if previous batch in flight
   */
  virtual bool is_ready() const = 0;
  
  /**
   * @brief Get maximum payload size
   * 
   * Maximum bytes that can be transmitted in send() call without truncation.
   * Equals max_packet_size - header_size from config.
   * 
   * @return Maximum payload bytes per packet
   */
  virtual size_t max_payload_size() const = 0;
  
  /**
   * @brief Get cumulative transmission statistics
   * 
   * Returns current counter values. Counters accumulate across all
   * send() calls since creation or last reset_stats().
   * 
   * @return Statistics struct with packet/byte/drop counts
   */
  virtual TransmissionStats get_stats() const = 0;
  
  /**
   * @brief Reset statistics counters to zero
   * 
   * Clears all cumulative counters (packets_sent, bytes_transmitted,
   * frames_dropped). Does not affect sender state or readiness.
   */
  virtual void reset_stats() = 0;
  
  /**
   * @brief Factory method to create sender instance
   * 
   * Initializes all low-level subsystems:
   * - Resolves port ID from interface name
   * - Parses network addresses (MAC, IPv4)
   * - Builds UDP/IP/Ethernet packet header template on GPU
   * - Allocates CUDA resources (streams, events, GPU buffers)
   * - Initializes DPDK burst manager
   * 
   * @param config Sender configuration (must be validated first)
   * @return Unique pointer to sender instance
   * @throws InvalidConfigException if addresses invalid
   * @throws NetworkInitException if interface not found or DPDK fails
   * @throws CudaInitException if CUDA initialization fails
   */
  static std::unique_ptr<IGpuDirectNetworkSender> create(const SenderConfig& config);
};

}  // namespace holoscan::ops
