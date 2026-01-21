/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/tensor_network_tx_op.h"
#include <sender_config.h>
#include <gpu_direct_exceptions.h>
#include <cuda_runtime.h>
#include <algorithm>
#include <vector>

namespace holoscan::ops {

namespace {
  constexpr size_t DEBUG_BYTES_TO_LOG = 32;
}

void TensorNetworkTxOp::setup(OperatorSpec& spec) {
  spec.input<std::shared_ptr<holoscan::Tensor>>("tensor_in");
  
  spec.param<std::string>(params_.interface_name_, "interface_name", "Interface Name",
             "Name of NIC interface from advanced_network config");
  spec.param<uint16_t>(params_.queue_id_, "queue_id", "Queue ID",
             "TX queue ID (must match advanced_network config)", 0);
  spec.param<std::string>(params_.ip_src_addr_, "ip_src_addr", "Source IP", "Source IP address");
  spec.param<std::string>(params_.ip_dst_addr_, "ip_dst_addr", "Destination IP", 
             "Destination IP address");
  spec.param<std::string>(params_.eth_dst_addr_, "eth_dst_addr", "Destination MAC", 
             "Destination MAC address");
  spec.param<uint16_t>(params_.udp_src_port_, "udp_src_port", "Source Port", 
             "Source UDP port", 5000);
  spec.param<uint16_t>(params_.udp_dst_port_, "udp_dst_port", "Destination Port", 
             "Destination UDP port", 5001);
  spec.param<uint16_t>(params_.header_size_, "header_size", "Header Size",
             "Size of packet headers (minimum 42 for Eth+IP+UDP)", 64);
  spec.param<uint16_t>(params_.max_packet_size_, "max_packet_size", "Max Packet Size",
             "Maximum packet size including headers", 9000);
}

void TensorNetworkTxOp::initialize() {
  HOLOSCAN_LOG_INFO("TensorNetworkTxOp::initialize()");
  Operator::initialize();

  // Build sender configuration from YAML parameters
  SenderConfig config;
  config.interface_name = params_.interface_name_.get();
  config.queue_id = params_.queue_id_.get();
  config.ip_src_addr = params_.ip_src_addr_.get();
  config.ip_dst_addr = params_.ip_dst_addr_.get();
  config.eth_dst_addr = params_.eth_dst_addr_.get();
  config.udp_src_port = params_.udp_src_port_.get();
  config.udp_dst_port = params_.udp_dst_port_.get();
  config.header_size = params_.header_size_.get();
  config.max_packet_size = params_.max_packet_size_.get();
  
  // Validate configuration
  try {
    config.validate();
  } catch (const InvalidConfigException& e) {
    HOLOSCAN_LOG_ERROR("Configuration validation failed: {}", e.what());
    throw;
  }
  
  // Create sender (may throw specific exceptions)
  try {
    sender_ = IGpuDirectNetworkSender::create(config);
    max_payload_size_ = sender_->max_payload_size();
    
    HOLOSCAN_LOG_INFO("TensorNetworkTxOp initialized: interface={}, dst={}:{}, "
                      "max_payload={} bytes, GPU-only mode",
                      config.interface_name, config.ip_dst_addr, config.udp_dst_port,
                      max_payload_size_);
  } catch (const NetworkInitException& e) {
    HOLOSCAN_LOG_ERROR("Network initialization failed: {}", e.what());
    throw;
  } catch (const CudaInitException& e) {
    HOLOSCAN_LOG_ERROR("CUDA initialization failed: {}", e.what());
    throw;
  }
}

void TensorNetworkTxOp::compute(InputContext& op_input, OutputContext& op_output,
                                ExecutionContext& context) {
  // Check sender readiness
  if (!sender_->is_ready()) {
    HOLOSCAN_LOG_WARN("Skipping frame, previous transmission still in flight");
    return;
  }

  // Receive and validate tensor
  auto tensor = receive_and_validate_tensor(op_input);
  if (!tensor.has_value()) {
    return;
  }

  log_tensor_debug_info(tensor.value());

  // Validate and adjust tensor size
  void* tensor_data = tensor.value()->data();
  size_t tensor_bytes = validate_and_adjust_tensor_size(tensor.value()->nbytes());

  // Send to network
  try {
    sender_->send(tensor_data, tensor_bytes);
    HOLOSCAN_LOG_INFO("✓ Sent {} bytes to network", tensor_bytes);
  } catch (const NotReadyException& e) {
    HOLOSCAN_LOG_ERROR("Dropping frame, batch {} still in flight (unexpected NotReadyException)",
                       e.batch_index());
  } catch (const CudaInitException& e) {
    HOLOSCAN_LOG_ERROR("CUDA error during transmission: {}", e.what());
  } catch (const NetworkInitException& e) {
    HOLOSCAN_LOG_ERROR("Network error during transmission: {}", e.what());
  }
}

std::optional<std::shared_ptr<holoscan::Tensor>> 
TensorNetworkTxOp::receive_and_validate_tensor(InputContext& op_input) {
  auto maybe_tensor = op_input.receive<std::shared_ptr<holoscan::Tensor>>("tensor_in");
  if (!maybe_tensor) {
    HOLOSCAN_LOG_DEBUG("No input tensor received");
    return std::nullopt;
  }

  auto tensor = maybe_tensor.value();
  DLDevice dev = tensor->device();
  if (dev.device_type != kDLCUDA) {
    throw std::runtime_error("Input tensor must be on CUDA device for GPUDirect transmission");
  }

  return tensor;
}

void TensorNetworkTxOp::log_tensor_debug_info(const std::shared_ptr<holoscan::Tensor>& tensor) {
  size_t tensor_bytes = tensor->nbytes();
  std::vector<uint8_t> debug_data(std::min(DEBUG_BYTES_TO_LOG, tensor_bytes));
  cudaMemcpy(debug_data.data(), tensor->data(), debug_data.size(), cudaMemcpyDeviceToHost);
  HOLOSCAN_LOG_INFO("Received data (first {} bytes): {}", debug_data.size(),
                    fmt::join(debug_data, " "));
  HOLOSCAN_LOG_DEBUG("Transmitting tensor: {} bytes from GPU", tensor_bytes);
}

size_t TensorNetworkTxOp::validate_and_adjust_tensor_size(size_t tensor_bytes) {
  if (tensor_bytes > max_payload_size_) {
    HOLOSCAN_LOG_WARN("Tensor size {} exceeds max payload {}, truncating",
                      tensor_bytes, max_payload_size_);
    return max_payload_size_;
  }
  return tensor_bytes;
}

}  // namespace holoscan::ops
