/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <holoscan/holoscan.hpp>
#include <advanced_network/common.h>
#include <cuda_runtime.h>
#include <queue>
#include <cuda_resource_manager.h>
#include <network_utils.h>
#include <packet_builder.h>
#include <packet_burst_manager.h>

using namespace holoscan::advanced_network;

namespace holoscan::ops {

/**
 * @brief Configuration parameters for TensorNetworkTxOp
 * 
 * Groups all configuration parameters that can be set through YAML or code.
 * These are bound to the Holoscan framework in the setup() method.
 */
struct TensorNetworkTxParams {
  Parameter<std::string> interface_name_;     ///< NIC interface name from advanced_network config
  Parameter<std::string> ip_src_addr_;        ///< Source IP address (e.g., "192.168.10.10")
  Parameter<std::string> ip_dst_addr_;        ///< Destination IP address (e.g., "192.168.10.11")
  Parameter<std::string> eth_dst_addr_;       ///< Destination MAC address (e.g., "3C:6D:66:11:91:56")
  Parameter<uint16_t> udp_src_port_;          ///< Source UDP port number
  Parameter<uint16_t> udp_dst_port_;          ///< Destination UDP port number
  Parameter<uint16_t> header_size_;           ///< Size of packet headers (Eth+IP+UDP), typically 42 bytes
  Parameter<uint16_t> max_packet_size_;       ///< Maximum packet size including headers
  Parameter<uint32_t> batch_size_;            ///< Number of packets per batch (unused in current impl)
  Parameter<int> hds_;                        ///< Header-data split size (0 = GPU-only mode)
};

/**
 * @brief Operator to transmit Holoscan Tensor data over network using GPUDirect
 * 
 * This operator receives Holoscan Tensors containing GPU data and transmits them
 * to a remote host using the Advanced Network library with DPDK backend and GPU-only mode.
 * The tensor data is sent directly from GPU memory to NIC via GPUDirect, eliminating
 * CPU copies for maximum performance.
 * 
 * Key Features:
 * - Zero-copy GPU-to-NIC transmission via GPUDirect
 * - Asynchronous CUDA operations with event-based flow control
 * - Pre-built packet headers stored on GPU for efficiency
 * - UDP/IPv4/Ethernet packet construction
 * 
 * Data Flow:
 * 1. Receive tensor from upstream operator (GPU memory)
 * 2. Check if previous transmission completed (CUDA event query)
 * 3. Prepare packet burst from DPDK pool
 * 4. Copy pre-made header + tensor payload to packet buffer (GPU-to-GPU)
 * 5. Record CUDA event and enqueue for transmission
 * 6. Process completed transmissions and send packets to NIC
 * 
 * Thread-safety: Not thread-safe. Designed for single-threaded operator execution.
 */
class TensorNetworkTxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TensorNetworkTxOp)

  TensorNetworkTxOp() = default;
  ~TensorNetworkTxOp() override;

  /**
   * @brief Define operator specification and parameters
   * 
   * Called by Holoscan framework to register input ports and configuration parameters.
   * All parameters can be set via YAML configuration or programmatically.
   */
  void setup(OperatorSpec& spec) override;

  /**
   * @brief Initialize operator state and allocate resources
   * 
   * Called once after setup(). Performs:
   * - Port ID lookup from advanced_network interface name
   * - Address parsing (MAC, IPv4) using NetworkUtils
   * - GPU header buffer allocation and initialization
   * - Pre-built packet header creation using PacketBuilder
   * - CUDA resource manager initialization (streams/events)
   */
  void initialize() override;

  /**
   * @brief Process one input tensor and transmit over network
   * 
   * Called repeatedly by scheduler. Main transmission pipeline:
   * 1. Check CUDA readiness (previous batch completed?)
   * 2. Receive and validate input tensor (must be GPU memory)
   * 3. Validate tensor size against max payload
   * 4. Prepare TX burst from DPDK pool
   * 5. Populate packet data (header + payload) on GPU
   * 6. Enqueue transmission with CUDA event
   * 7. Process any completed transmissions and send to NIC
   */
  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override;

 private:
  //
  // Member Variables
  //

  /// Configuration parameters bound to Holoscan framework
  TensorNetworkTxParams params_;

  /// Runtime values extracted from config (cached for performance)
  uint16_t max_packet_size_ = 0;  ///< Maximum packet size including headers
  uint16_t header_size_ = 0;      ///< Size of Eth+IP+UDP headers (typically 42 bytes)

  /// Network state set during initialization
  int port_id_ = -1;       ///< DPDK port ID for the network interface
  uint16_t queue_id_ = 0;  ///< TX queue ID (currently fixed to 0)

  /// CUDA resource management (streams and events for async operations)
  CudaResourceManager cuda_manager_;

  /// Pre-built packet header stored on GPU (copied to each packet)
  /// Contains Ethernet, IP, and UDP headers ready for transmission
  void* gds_header_ = nullptr;

  /// Burst manager for TX packet lifecycle (allocation, queueing, transmission)
  std::unique_ptr<PacketBurstManager> burst_manager_;

  //
  // Helper Methods
  //

  /**
   * @brief Parse configuration parameters into NetworkConfig struct
   * 
   * Extracts values from params_ (which are bound to Holoscan Parameter<T>)
   * and returns a plain NetworkConfig struct for internal use.
   * 
   * @return NetworkConfig containing all network configuration values
   */
  NetworkConfig parse_network_config();
  
  //
  // Compute Pipeline Helper Methods
  // (These break down the compute() method into focused, testable steps)
  //

  /**
   * @brief Check if ready to start new transmission
   * 
   * Queries the current CUDA event to see if previous batch completed.
   * If not ready, logs warning and returns false.
   * 
   * @return true if ready for new transmission, false if previous still in-flight
   */
  bool is_ready_for_transmission();

  /**
   * @brief Receive and validate input tensor from upstream operator
   * 
   * Receives tensor from "tensor_in" port and validates:
   * - Tensor is present (not nullopt)
   * - Tensor data resides on CUDA device (required for GPUDirect)
   * 
   * @param op_input Input context for receiving data
   * @return Optional tensor (has value if valid, nullopt if not received)
   * @throws std::runtime_error if tensor is not on CUDA device
   */
  std::optional<std::shared_ptr<holoscan::Tensor>> receive_and_validate_tensor(InputContext& op_input);

  /**
   * @brief Log tensor debug information
   * 
   * Copies first few bytes from GPU to host and logs for debugging.
   * Useful for verifying data integrity.
   * 
   * @param tensor Tensor to log debug info for
   */
  void log_tensor_debug_info(const std::shared_ptr<holoscan::Tensor>& tensor);

  /**
   * @brief Validate and adjust tensor size to fit in one packet
   * 
   * Checks if tensor fits within max payload size (max_packet_size - header_size).
   * If too large, truncates and logs warning.
   * 
   * @param tensor_bytes Original tensor size in bytes
   * @return Adjusted size that fits in payload (may be truncated)
   */
  size_t validate_and_adjust_tensor_size(size_t tensor_bytes);
};

}  // namespace holoscan::ops
