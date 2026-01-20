/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/tensor_network_tx_op.h"
#include <arpa/inet.h>
#include <cstring>

namespace holoscan::ops {

// Network protocol constants
namespace {
  constexpr uint8_t DEFAULT_IP_TTL = 64;
  constexpr size_t DEBUG_BYTES_TO_LOG = 32;
  constexpr int NUM_CONCURRENT_BATCHES = 4;
}

TensorNetworkTxOp::~TensorNetworkTxOp() {
  HOLOSCAN_LOG_INFO("TensorNetworkTxOp shutting down");
  
  // CUDA resources automatically cleaned up by RAII wrappers
  // burst_manager_ automatically cleans up pending bursts
  
  // Free GPU header buffer
  if (gds_header_) {
    cudaFree(gds_header_);
  }
}

void TensorNetworkTxOp::setup(OperatorSpec& spec) {
  spec.input<std::shared_ptr<holoscan::Tensor>>("tensor_in");
  
  spec.param<std::string>(params_.interface_name_, "interface_name", "Interface Name",
             "Name of NIC interface from advanced_network config");
  spec.param<std::string>(params_.ip_src_addr_, "ip_src_addr", "Source IP", "Source IP address");
  spec.param<std::string>(params_.ip_dst_addr_, "ip_dst_addr", "Destination IP", "Destination IP address");
  spec.param<std::string>(params_.eth_dst_addr_, "eth_dst_addr", "Destination MAC", "Destination MAC address");
  spec.param<uint16_t>(params_.udp_src_port_, "udp_src_port", "Source Port", "Source UDP port", 5000);
  spec.param<uint16_t>(params_.udp_dst_port_, "udp_dst_port", "Destination Port", "Destination UDP port", 5001);
  spec.param<uint16_t>(params_.header_size_, "header_size", "Header Size",
             "Size of packet headers (Eth+IP+UDP)", 42);
  spec.param<uint16_t>(params_.max_packet_size_, "max_packet_size", "Max Packet Size",
             "Maximum packet size including headers", 9000);
  spec.param<uint32_t>(params_.batch_size_, "batch_size", "Batch Size",
             "Number of packets per batch", 64);
  spec.param<int>(params_.hds_, "split_boundary", "Header-Data Split",
             "Header-data split size (0 for GPU-only mode)", 0);
}

void TensorNetworkTxOp::initialize() {
  HOLOSCAN_LOG_INFO("TensorNetworkTxOp::initialize()");
  Operator::initialize();

  // Parse configuration into local NetworkConfig
  NetworkConfig config = parse_network_config();

  // Store runtime values needed in compute()
  max_packet_size_ = config.max_packet_size;
  header_size_ = config.header_size;

  // Get port ID from advanced_network
  port_id_ = get_port_id(config.interface_name);
  if (port_id_ == -1) {
    throw std::runtime_error(
        fmt::format("Invalid interface '{}' specified", config.interface_name));
  }

  // Parse addresses using utility functions
  std::array<uint8_t, 6> mac_bytes;
  NetworkUtils::parse_mac_address(config.eth_dst_addr, mac_bytes);

  // Parse IP addresses (network byte order for packet headers)
  uint32_t ip_src = NetworkUtils::parse_ipv4_address(config.ip_src_addr);
  uint32_t ip_dst = NetworkUtils::parse_ipv4_address(config.ip_dst_addr);

  // CUDA streams and events are automatically created by RAII constructors
  // (streams_ and events_ arrays are initialized in member initialization)

  // For GPU-only mode, prepare header template on GPU
  cudaMalloc(&gds_header_, config.header_size);
  cudaMemset(gds_header_, 0, config.header_size);

  // Populate packet headers using PacketBuilder from common library
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    config.max_packet_size,
    config.udp_src_port,
    config.udp_dst_port,
    ip_src,
    ip_dst,
    mac_bytes,
    DEFAULT_IP_TTL
  );

  // Copy the pre-made header to GPU
  cudaMemcpy(gds_header_, reinterpret_cast<void*>(&pkt), sizeof(pkt), cudaMemcpyDefault);
  
  // CUDA resource manager already initialized in constructor (with default concurrent count)
  // Initialize burst manager for TX packet lifecycle
  burst_manager_ = std::make_unique<PacketBurstManager>(
    port_id_, queue_id_, header_size_, max_packet_size_
  );

  HOLOSCAN_LOG_INFO("TensorNetworkTxOp initialized: port_id={}, dst={}:{}, GPU-only mode={}",
                    port_id_, config.ip_dst_addr, config.udp_dst_port, 
                    (config.header_data_split == 0));
}

NetworkConfig TensorNetworkTxOp::parse_network_config() {
  NetworkConfig config;
  config.interface_name = params_.interface_name_.get();
  config.ip_src_addr = params_.ip_src_addr_.get();
  config.ip_dst_addr = params_.ip_dst_addr_.get();
  config.eth_dst_addr = params_.eth_dst_addr_.get();
  config.udp_src_port = params_.udp_src_port_.get();
  config.udp_dst_port = params_.udp_dst_port_.get();
  config.header_size = params_.header_size_.get();
  config.max_packet_size = params_.max_packet_size_.get();
  config.batch_size = params_.batch_size_.get();
  config.header_data_split = params_.hds_.get();
  return config;
}

void TensorNetworkTxOp::compute(InputContext& op_input, OutputContext& op_output,
                                ExecutionContext& context) {
  if (!is_ready_for_transmission()) {
    return;
  }

  auto tensor = receive_and_validate_tensor(op_input);
  if (!tensor.has_value()) {
    return;
  }

  log_tensor_debug_info(tensor.value());

  void* tensor_data = tensor.value()->data();
  size_t tensor_bytes = validate_and_adjust_tensor_size(tensor.value()->nbytes());

  int num_packets = 1;
  BurstParams* burst = burst_manager_->prepare_tx_burst(num_packets);
  if (burst == nullptr) {
    return;
  }

  cudaStream_t stream = cuda_manager_.get_stream();
  if (!burst_manager_->populate_tx_packet_data(burst, gds_header_, tensor_data, 
                                               tensor_bytes, num_packets, stream)) {
    return;
  }

  // Get CUDA event for synchronization
  cudaEvent_t event = cuda_manager_.get_event();
  // Record the event and advance the CUDA manager state
  cuda_manager_.record_and_advance();
  burst_manager_->enqueue_tx_burst(burst, event);
  
  int sent_count = burst_manager_->send_ready_bursts();
  if (sent_count > 0) {
    HOLOSCAN_LOG_INFO("✓ Sent {} packets ({} bytes)", sent_count, tensor_bytes);
  }
}

bool TensorNetworkTxOp::is_ready_for_transmission() {
  if (!cuda_manager_.is_ready()) {
    HOLOSCAN_LOG_WARN("Previous TX batch {} still in flight, skipping", 
                      cuda_manager_.current_index());
    return false;
  }
  return true;
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
  size_t max_payload = max_packet_size_ - header_size_;
  if (tensor_bytes > max_payload) {
    HOLOSCAN_LOG_WARN("Tensor size {} exceeds max payload {}, truncating",
                      tensor_bytes, max_payload);
    return max_payload;
  }
  return tensor_bytes;
}

}  // namespace holoscan::ops
