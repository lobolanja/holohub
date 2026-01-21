/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include "receiver_config.h"

namespace holoscan::ops {

/**
 * @brief Reception statistics
 * 
 * Cumulative counters reset only by reset_stats().
 */
struct ReceptionStats {
  uint64_t packets_received = 0;  ///< Total packets received from NIC
  uint64_t bytes_received = 0;    ///< Total payload bytes received
  uint64_t polls_attempted = 0;   ///< Total receive() calls
  uint64_t empty_polls = 0;       ///< receive() calls that returned no data
};

/**
 * @brief Received packet data
 * 
 * Contains GPU pointer to payload (headers stripped) and size.
 * Caller must call free_received_data() to release memory.
 */
struct ReceivedData {
  void* gpu_payload;     ///< GPU pointer to payload data (headers stripped)
  size_t payload_bytes;  ///< Size of payload in bytes
};

/**
 * @brief Interface for GPU Direct network reception
 * 
 * Provides high-level API for receiving GPU memory buffers from network
 * using GPUDirect and DPDK. Hides all low-level CUDA, DPDK, and packet
 * parsing details from callers.
 * 
 * Features:
 * - Zero-copy reception: NIC → GPU via GPUDirect (no CPU involvement)
 * - Simple API: Single receive() call returns GPU pointer
 * - Automatic header stripping: Returns only payload data
 * - Async CUDA operations: Internal stream/event management
 * - Multi-queue polling: Polls all RX queues automatically
 * - Cumulative statistics tracking
 * 
 * Memory management:
 * - Facade allocates new GPU buffer for each received packet
 * - Caller owns the buffer and MUST call free_received_data()
 * - Facade handles internal burst cleanup asynchronously
 * 
 * Configuration:
 * - GPU-only mode: No header-data split, entire packet on GPU
 * - Hardcoded concurrent slots: 4 (for async CUDA operations)
 * 
 * Thread-safety: NOT thread-safe. Use from single thread only.
 * 
 * Example usage:
 * ```cpp
 * // Initialization
 * ReceiverConfig config;
 * config.interface_name = "rx_port";
 * config.header_size = 64;
 * config.max_packet_size = 1064;
 * config.gpu_device = 0;
 * config.validate();  // Throws on invalid config
 * 
 * auto receiver = IGpuDirectNetworkReceiver::create(config);
 * 
 * // Runtime reception
 * auto maybe_data = receiver->receive();
 * if (maybe_data.has_value()) {
 *   auto data = maybe_data.value();
 *   // Use data.gpu_payload (size: data.payload_bytes)
 *   
 *   // MUST free when done
 *   receiver->free_received_data(data.gpu_payload);
 * } else {
 *   // No data available (NOT_READY status)
 * }
 * ```
 */
class IGpuDirectNetworkReceiver {
 public:
  virtual ~IGpuDirectNetworkReceiver() = default;
  
  /**
   * @brief Receive packet from network
   * 
   * Polls all RX queues and returns first available packet.
   * Automatically frees completed DPDK bursts from previous calls.
   * Allocates new GPU buffer, copies payload (strips headers), returns pointer.
   * 
   * Non-blocking: Returns std::nullopt if no data available.
   * 
   * @return ReceivedData with GPU pointer and size, or std::nullopt if no data
   * @throws CudaInitException if CUDA operations fail
   * @throws NetworkInitException if network operations fail
   * 
   * Side effects:
   * - Frees DPDK bursts from previous receive() calls (async CUDA completed)
   * - Allocates new GPU memory (caller must free via free_received_data())
   * - Updates packets_received, bytes_received, polls_attempted, empty_polls stats
   * 
   * Memory contract:
   * - Caller MUST call free_received_data() with returned gpu_payload
   * - Failure to free causes memory leak
   */
  virtual std::optional<ReceivedData> receive() = 0;
  
  /**
   * @brief Free GPU memory allocated by receive()
   * 
   * Releases GPU buffer allocated during receive() call.
   * MUST be called for every non-nullopt value returned by receive().
   * 
   * @param gpu_payload Pointer returned by receive()
   * @throws CudaInitException if cudaFree fails
   * 
   * Thread-safety: Must be called from same thread as receive()
   */
  virtual void free_received_data(void* gpu_payload) = 0;
  
  /**
   * @brief Get maximum payload size
   * 
   * Maximum bytes that can be received per packet (after header stripping).
   * Equals max_packet_size - header_size from config.
   * 
   * @return Maximum payload bytes per packet
   */
  virtual size_t max_payload_size() const = 0;
  
  /**
   * @brief Get cumulative reception statistics
   * 
   * Returns current counter values. Counters accumulate across all
   * receive() calls since creation or last reset_stats().
   * 
   * @return Statistics struct with packet/byte/poll counts
   */
  virtual ReceptionStats get_stats() const = 0;
  
  /**
   * @brief Reset statistics counters to zero
   * 
   * Clears all cumulative counters (packets_received, bytes_received,
   * polls_attempted, empty_polls). Does not affect receiver state.
   */
  virtual void reset_stats() = 0;
  
  /**
   * @brief Factory method to create receiver instance
   * 
   * Initializes all low-level subsystems:
   * - Resolves port ID from interface name
   * - Sets GPU device
   * - Initializes CUDA resource manager (streams/events)
   * - Queries number of RX queues
   * 
   * @param config Receiver configuration (must be validated first)
   * @return Unique pointer to receiver instance
   * @throws InvalidConfigException if config invalid
   * @throws NetworkInitException if interface not found
   * @throws CudaInitException if CUDA initialization fails
   */
  static std::unique_ptr<IGpuDirectNetworkReceiver> create(const ReceiverConfig& config);
};

}  // namespace holoscan::ops
