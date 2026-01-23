/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <holoscan/holoscan.hpp>
#include <connext_ano_lib/gpu_direct_network_sender.h>
#include <optional>

namespace holoscan::ops {

/**
 * @brief Configuration parameters for TensorNetworkTxOp
 * 
 * Groups all configuration parameters that can be set through YAML or code.
 * These are bound to the Holoscan framework in the setup() method.
 */
struct TensorNetworkTxParams {
  Parameter<std::string> interface_name_;     ///< NIC interface name from advanced_network config
  Parameter<uint16_t> queue_id_;              ///< TX queue ID (must match advanced_network config)
  Parameter<std::string> ip_src_addr_;        ///< Source IP address (e.g., "192.168.10.10")
  Parameter<std::string> ip_dst_addr_;        ///< Destination IP address (e.g., "192.168.10.11")
  Parameter<std::string> eth_dst_addr_;       ///< Destination MAC address (e.g., "3C:6D:66:11:91:56")
  Parameter<uint16_t> udp_src_port_;          ///< Source UDP port number
  Parameter<uint16_t> udp_dst_port_;          ///< Destination UDP port number
  Parameter<uint16_t> header_size_;           ///< Size of packet headers (minimum 42 for Eth+IP+UDP)
  Parameter<uint16_t> max_packet_size_;       ///< Maximum packet size including headers
};

/**
 * @brief Operator to transmit Holoscan Tensor data over network using GPUDirect
 * 
 * This operator receives Holoscan Tensors containing GPU data and transmits them
 * to a remote host using GPUDirect and DPDK. The tensor data is sent directly from
 * GPU memory to NIC via GPUDirect, eliminating CPU copies for maximum performance.
 * 
 * Key Features:
 * - Zero-copy GPU-to-NIC transmission via GPUDirect
 * - Simplified interface via GpuDirectNetworkSender facade
 * - Automatic size validation and truncation
 * - GPU-only mode (no header-data split)
 * 
 * Data Flow:
 * 1. Receive tensor from upstream operator (GPU memory)
 * 2. Check if sender is ready for transmission
 * 3. Validate tensor size and truncate if needed
 * 4. Send GPU pointer to network sender
 * 
 * Thread-safety: Not thread-safe. Designed for single-threaded operator execution.
 */
class TensorNetworkTxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TensorNetworkTxOp)

  TensorNetworkTxOp() = default;
  ~TensorNetworkTxOp() override = default;

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
   * Called once after setup(). Creates the GPU Direct network sender
   * with configuration from YAML parameters.
   * 
   * @throws InvalidConfigException if configuration invalid
   * @throws NetworkInitException if network initialization fails
   * @throws CudaInitException if CUDA initialization fails
   */
  void initialize() override;

  /**
   * @brief Process one input tensor and transmit over network
   * 
   * Called repeatedly by scheduler. Main transmission pipeline:
   * 1. Check sender readiness (previous batch completed?)
   * 2. Receive and validate input tensor (must be GPU memory)
   * 3. Validate tensor size and log truncation warning if needed
   * 4. Send GPU pointer to network sender
   * 5. Handle NotReadyException if sender unexpectedly not ready
   */
  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override;

 private:
  //
  // Member Variables
  //

  /// Configuration parameters bound to Holoscan framework
  TensorNetworkTxParams params_;

  /// GPU Direct network sender (encapsulates all low-level details)
  std::unique_ptr<IGpuDirectNetworkSender> sender_;

  /// Maximum payload size for validation (cached from sender)
  size_t max_payload_size_ = 0;

  //
  // Helper Methods
  //

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
  std::optional<std::shared_ptr<holoscan::Tensor>> receive_and_validate_tensor(
      InputContext& op_input);

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
   * Checks if tensor fits within max payload size.
   * If too large, truncates and logs warning.
   * 
   * @param tensor_bytes Original tensor size in bytes
   * @return Adjusted size that fits in payload (may be truncated)
   */
  size_t validate_and_adjust_tensor_size(size_t tensor_bytes);
};

}  // namespace holoscan::ops
