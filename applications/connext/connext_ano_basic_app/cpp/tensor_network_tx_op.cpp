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
  constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
  constexpr uint8_t IP_HEADER_LENGTH_WORDS = 5;  // 20 bytes / 4
  constexpr uint8_t DEFAULT_IP_TTL = 64;
  constexpr size_t DEBUG_BYTES_TO_LOG = 32;
}

TensorNetworkTxOp::~TensorNetworkTxOp() {
  HOLOSCAN_LOG_INFO("TensorNetworkTxOp shutting down");
  
  // CUDA resources automatically cleaned up by RAII wrappers
  
  // Free GPU header buffer
  if (gds_header_) {
    cudaFree(gds_header_);
  }
  
  // Free any pending burst buffers
  while (!out_q_.empty()) {
    free_tx_burst(out_q_.front().msg);
    out_q_.pop();
  }
}

void TensorNetworkTxOp::setup(OperatorSpec& spec) {
  spec.input<std::shared_ptr<holoscan::Tensor>>("tensor_in");
  
  spec.param<std::string>(interface_name_, "interface_name", "Interface Name",
             "Name of NIC interface from advanced_network config");
  spec.param<std::string>(ip_src_addr_, "ip_src_addr", "Source IP", "Source IP address");
  spec.param<std::string>(ip_dst_addr_, "ip_dst_addr", "Destination IP", "Destination IP address");
  spec.param<std::string>(eth_dst_addr_, "eth_dst_addr", "Destination MAC", "Destination MAC address");
  spec.param<uint16_t>(udp_src_port_, "udp_src_port", "Source Port", "Source UDP port", 5000);
  spec.param<uint16_t>(udp_dst_port_, "udp_dst_port", "Destination Port", "Destination UDP port", 5001);
  spec.param<uint16_t>(header_size_, "header_size", "Header Size",
             "Size of packet headers (Eth+IP+UDP)", 42);
  spec.param<uint16_t>(max_packet_size_, "max_packet_size", "Max Packet Size",
             "Maximum packet size including headers", 9000);
  spec.param<uint32_t>(batch_size_, "batch_size", "Batch Size",
             "Number of packets per batch", 64);
  spec.param<int>(hds_, "split_boundary", "Header-Data Split",
             "Header-data split size (0 for GPU-only mode)", 0);
}

void TensorNetworkTxOp::initialize() {
  HOLOSCAN_LOG_INFO("TensorNetworkTxOp::initialize()");
  Operator::initialize();

  // Parse configuration into NetworkConfig struct
  parse_network_config();

  // Get port ID from advanced_network
  port_id_ = get_port_id(config_.interface_name);
  if (port_id_ == -1) {
    throw std::runtime_error(
        fmt::format("Invalid interface '{}' specified", config_.interface_name));
  }

  // Parse addresses using utility functions
  std::array<uint8_t, 6> mac_bytes;
  NetworkUtils::parse_mac_address(config_.eth_dst_addr, mac_bytes);
  std::memcpy(eth_dst_, mac_bytes.data(), mac_bytes.size());

  // Parse IP addresses (network byte order for packet headers)
  ip_src_ = NetworkUtils::parse_ipv4_address(config_.ip_src_addr);
  ip_dst_ = NetworkUtils::parse_ipv4_address(config_.ip_dst_addr);

  // CUDA streams and events are automatically created by RAII constructors
  // (streams_ and events_ arrays are initialized in member initialization)

  // For GPU-only mode, prepare header template on GPU
  cudaMalloc(&gds_header_, config_.header_size);
  cudaMemset(gds_header_, 0, config_.header_size);

  // Populate packet headers on host
  populate_packet_headers(config_);
  
  // Convert IPs to host order for advanced_network API (if needed later)
  ip_src_ = ntohl(ip_src_);
  ip_dst_ = ntohl(ip_dst_);

  // Copy the pre-made header to GPU
  cudaMemcpy(gds_header_, reinterpret_cast<void*>(&pkt_), sizeof(pkt_), cudaMemcpyDefault);

  HOLOSCAN_LOG_INFO("TensorNetworkTxOp initialized: port_id={}, dst={}:{}, GPU-only mode={}",
                    port_id_, config_.ip_dst_addr, config_.udp_dst_port, 
                    (config_.header_data_split == 0));
}

void TensorNetworkTxOp::populate_packet_headers(const NetworkConfig& config) {
  // Ethernet header
  memcpy(pkt_.eth.h_dest, eth_dst_, sizeof(pkt_.eth.h_dest));
  // Source MAC will be set by NIC offload
  memset(pkt_.eth.h_source, 0, sizeof(pkt_.eth.h_source));
  pkt_.eth.h_proto = htons(ETHERTYPE_IPV4);

  // IP header  
  uint16_t ip_len = config.max_packet_size - sizeof(pkt_.eth);
  
  pkt_.ip.version = 4;
  pkt_.ip.ihl = IP_HEADER_LENGTH_WORDS;
  pkt_.ip.tos = 0;
  pkt_.ip.tot_len = htons(ip_len);
  pkt_.ip.id = 0;
  pkt_.ip.frag_off = 0;
  pkt_.ip.ttl = DEFAULT_IP_TTL;
  pkt_.ip.protocol = IPPROTO_UDP;
  pkt_.ip.check = 0;  // Offloaded to NIC
  pkt_.ip.saddr = ip_src_;
  pkt_.ip.daddr = ip_dst_;

  // UDP header
  pkt_.udp.source = htons(config.udp_src_port);
  pkt_.udp.dest = htons(config.udp_dst_port);
  pkt_.udp.len = htons(ip_len - sizeof(pkt_.ip));
  pkt_.udp.check = 0;  // Offloaded to NIC
}

void TensorNetworkTxOp::parse_network_config() {
  config_.interface_name = interface_name_.get();
  config_.ip_src_addr = ip_src_addr_.get();
  config_.ip_dst_addr = ip_dst_addr_.get();
  config_.eth_dst_addr = eth_dst_addr_.get();
  config_.udp_src_port = udp_src_port_.get();
  config_.udp_dst_port = udp_dst_port_.get();
  config_.header_size = header_size_.get();
  config_.max_packet_size = max_packet_size_.get();
  config_.batch_size = batch_size_.get();
  config_.header_data_split = hds_.get();
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
  BurstParams* burst = nullptr;
  
  if (!prepare_tx_burst(burst, num_packets)) {
    return;
  }

  if (!populate_packet_data(burst, tensor_data, tensor_bytes, num_packets)) {
    return;
  }

  enqueue_transmission(burst);
  process_pending_transmissions(tensor_bytes);
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
  size_t max_payload = config_.max_packet_size - config_.header_size;
  if (tensor_bytes > max_payload) {
    HOLOSCAN_LOG_WARN("Tensor size {} exceeds max payload {}, truncating",
                      tensor_bytes, max_payload);
    return max_payload;
  }
  return tensor_bytes;
}

bool TensorNetworkTxOp::prepare_tx_burst(BurstParams*& burst, int num_packets) {
  burst = create_tx_burst_params();
  set_header(burst, port_id_, queue_id_, num_packets, 1);

  if (!is_tx_burst_available(burst)) {
    HOLOSCAN_LOG_WARN("TX burst not available, skipping");
    free_tx_metadata(burst);
    return false;
  }

  Status ret = get_tx_packet_burst(burst);
  if (ret != Status::SUCCESS) {
    HOLOSCAN_LOG_ERROR("Failed to get TX packet burst: {}", static_cast<int>(ret));
    free_tx_metadata(burst);
    return false;
  }

  return true;
}

bool TensorNetworkTxOp::populate_packet_data(BurstParams* burst, void* tensor_data, 
                                              size_t tensor_bytes, int num_packets) {
  cudaStream_t stream = cuda_manager_.get_stream();
  
  for (int pkt_idx = 0; pkt_idx < num_packets; pkt_idx++) {
    void* gpu_pkt_ptr = get_segment_packet_ptr(burst, 0, pkt_idx);
    
    // Copy pre-made header from GPU template
    cudaMemcpyAsync(gpu_pkt_ptr, gds_header_, config_.header_size,
                    cudaMemcpyDeviceToDevice, stream);
    
    // Copy tensor payload after header
    cudaMemcpyAsync(static_cast<uint8_t*>(gpu_pkt_ptr) + config_.header_size,
                    tensor_data,
                    tensor_bytes,
                    cudaMemcpyDeviceToDevice,
                    stream);
    
    // Set total packet length
    uint16_t total_len = config_.header_size + tensor_bytes;
    Status ret = set_packet_lengths(burst, pkt_idx, {static_cast<int>(total_len)});
    if (ret != Status::SUCCESS) {
      HOLOSCAN_LOG_ERROR("Failed to set packet length for packet {}", pkt_idx);
      free_all_packets_and_burst_tx(burst);
      return false;
    }
  }
  return true;
}

void TensorNetworkTxOp::enqueue_transmission(BurstParams* burst) {
  cudaEvent_t event = cuda_manager_.get_event();
  cuda_manager_.record_and_advance();
  out_q_.push(TxMsg{burst, event});
}

void TensorNetworkTxOp::process_pending_transmissions(size_t last_tensor_bytes) {
  while (!out_q_.empty()) {
    const auto& front = out_q_.front();
    if (cudaEventQuery(front.evt) == cudaSuccess) {
      Status ret = send_tx_burst(front.msg);
      if (ret != Status::SUCCESS) {
        HOLOSCAN_LOG_ERROR("Failed to send TX burst: {}", static_cast<int>(ret));
      } else {
        HOLOSCAN_LOG_INFO("! Sent {} packets ({} bytes)",
                           get_num_packets(front.msg), last_tensor_bytes);
      }
      out_q_.pop();
    } else {
      // Not ready yet, stop checking
      break;
    }
  }
}

}  // namespace holoscan::ops
