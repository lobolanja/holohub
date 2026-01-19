/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/tensor_network_tx_op.h"
#include <arpa/inet.h>
#include <cstring>

namespace holoscan::ops {

TensorNetworkTxOp::~TensorNetworkTxOp() {
  HOLOSCAN_LOG_INFO("TensorNetworkTxOp shutting down");
  
  // Cleanup CUDA resources
  for (int i = 0; i < num_concurrent; i++) {
    if (streams_[i]) cudaStreamDestroy(streams_[i]);
    if (events_[i]) cudaEventDestroy(events_[i]);
  }
  
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

  // Get port ID from advanced_network
  port_id_ = get_port_id(interface_name_.get());
  if (port_id_ == -1) {
    throw std::runtime_error(
        fmt::format("Invalid interface '{}' specified", interface_name_.get()));
  }

  // Parse MAC address
  format_eth_addr(eth_dst_, eth_dst_addr_.get());

  // Parse IP addresses and convert to network byte order first
  inet_pton(AF_INET, ip_src_addr_.get().c_str(), &ip_src_);
  inet_pton(AF_INET, ip_dst_addr_.get().c_str(), &ip_dst_);

  // Create CUDA streams and events for async operation
  for (int i = 0; i < num_concurrent; i++) {
    cudaStreamCreate(&streams_[i]);
    cudaEventCreate(&events_[i]);
  }

  // For GPU-only mode, prepare header template on GPU
  cudaMalloc(&gds_header_, header_size_.get());
  cudaMemset(gds_header_, 0, header_size_.get());

  // Populate packet headers on host
  populate_packet_headers();
  
  // Convert IPs to host order for advanced_network API (if needed later)
  ip_src_ = ntohl(ip_src_);
  ip_dst_ = ntohl(ip_dst_);

  // Copy the pre-made header to GPU
  cudaMemcpy(gds_header_, reinterpret_cast<void*>(&pkt_), sizeof(pkt_), cudaMemcpyDefault);

  HOLOSCAN_LOG_INFO("TensorNetworkTxOp initialized: port_id={}, dst={}:{}, GPU-only mode={}",
                    port_id_, ip_dst_addr_.get(), udp_dst_port_.get(), (hds_.get() == 0));
}

void TensorNetworkTxOp::format_eth_addr(char* dst, const std::string& addr_str) {
  unsigned int values[6];
  if (sscanf(addr_str.c_str(), "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) != 6) {
    throw std::runtime_error(fmt::format("Invalid MAC address format: {}", addr_str));
  }
  for (int i = 0; i < 6; i++) {
    dst[i] = static_cast<char>(values[i]);
  }
}

void TensorNetworkTxOp::populate_packet_headers() {
  // Ethernet header
  memcpy(pkt_.eth.h_dest, eth_dst_, sizeof(pkt_.eth.h_dest));
  // Source MAC will be set by NIC offload
  memset(pkt_.eth.h_source, 0, sizeof(pkt_.eth.h_source));
  pkt_.eth.h_proto = htons(0x0800);  // IPv4

  // IP header  
  uint16_t ip_len = max_packet_size_.get() - sizeof(pkt_.eth);
  
  pkt_.ip.version = 4;
  pkt_.ip.ihl = 20 / 4;  // 5 (20 bytes)
  pkt_.ip.tos = 0;
  pkt_.ip.tot_len = htons(ip_len);
  pkt_.ip.id = 0;
  pkt_.ip.frag_off = 0;
  pkt_.ip.ttl = 64;  // Standard TTL
  pkt_.ip.protocol = IPPROTO_UDP;
  pkt_.ip.check = 0;  // Offloaded to NIC
  pkt_.ip.saddr = ip_src_;
  pkt_.ip.daddr = ip_dst_;

  // UDP header
  pkt_.udp.source = htons(udp_src_port_.get());
  pkt_.udp.dest = htons(udp_dst_port_.get());
  pkt_.udp.len = htons(ip_len - sizeof(pkt_.ip));
  pkt_.udp.check = 0;  // Offloaded to NIC
}

void TensorNetworkTxOp::compute(InputContext& op_input, OutputContext& op_output,
                                ExecutionContext& context) {
  Status ret;
  
  // Check if previous batch is still in flight
  if (cudaEventQuery(events_[cur_idx_]) != cudaSuccess) {
    HOLOSCAN_LOG_WARN("Previous TX batch {} still in flight, skipping", cur_idx_);
    return;
  }

  // Receive input tensor
  auto maybe_tensor = op_input.receive<std::shared_ptr<holoscan::Tensor>>("tensor_in");
  if (!maybe_tensor) {
    HOLOSCAN_LOG_DEBUG("No input tensor received");
    return;
  }
  auto in_tensor = maybe_tensor.value();

  // Validate tensor is on GPU
  DLDevice dev = in_tensor->device();
  if (dev.device_type != kDLCUDA) {
    throw std::runtime_error("Input tensor must be on CUDA device for GPUDirect transmission");
  }

  // Get tensor data pointer and size
  void* tensor_data = in_tensor->data();
  size_t tensor_bytes = in_tensor->nbytes();
  
  // Debug: Verify received data (the data is copied to the GPU only for debug purposes)
  std::vector<uint8_t> debug_data(std::min(32UL, tensor_bytes));
  cudaMemcpy(debug_data.data(), in_tensor->data(), debug_data.size(), cudaMemcpyDeviceToHost);
  HOLOSCAN_LOG_INFO("Received data (first {} bytes): {}", debug_data.size(),
                    fmt::join(debug_data, " "));
  
  HOLOSCAN_LOG_DEBUG("Transmitting tensor: {} bytes from GPU", tensor_bytes);

  // For simplicity, send tensor in a single packet (assuming it fits)
  // In production, you'd fragment across multiple packets
  int num_packets = 1;
  
  if (tensor_bytes > (max_packet_size_.get() - header_size_.get())) {
    HOLOSCAN_LOG_WARN("Tensor size {} exceeds max payload {}, truncating",
                      tensor_bytes, max_packet_size_.get() - header_size_.get());
    tensor_bytes = max_packet_size_.get() - header_size_.get();
  }

  // Create burst parameters
  auto burst = create_tx_burst_params();
  
  // Set header: port_id, queue_id, num_packets, num_segments (1 for GPU-only)
  set_header(burst, port_id_, queue_id_, num_packets, 1);

  // Check if burst is available
  if (!is_tx_burst_available(burst)) {
    HOLOSCAN_LOG_WARN("TX burst not available, skipping");
    free_tx_metadata(burst);
    return;
  }

  // Get packet burst from advanced_network
  ret = get_tx_packet_burst(burst);
  if (ret != Status::SUCCESS) {
    HOLOSCAN_LOG_ERROR("Failed to get TX packet burst: {}", static_cast<int>(ret));
    free_tx_metadata(burst);
    return;
  }

  // GPU-only mode: copy header + payload to GPU packet buffer
  for (int pkt_idx = 0; pkt_idx < num_packets; pkt_idx++) {
    void* gpu_pkt_ptr = get_segment_packet_ptr(burst, 0, pkt_idx);
    
    // Copy pre-made header from GPU template
    cudaMemcpyAsync(gpu_pkt_ptr, gds_header_, header_size_.get(),
                    cudaMemcpyDeviceToDevice, streams_[cur_idx_]);
    
    // Copy tensor payload after header
    cudaMemcpyAsync(static_cast<uint8_t*>(gpu_pkt_ptr) + header_size_.get(),
                    tensor_data,
                    tensor_bytes,
                    cudaMemcpyDeviceToDevice,
                    streams_[cur_idx_]);
    
    // Set total packet length
    uint16_t total_len = header_size_.get() + tensor_bytes;
    if ((ret = set_packet_lengths(burst, pkt_idx, {static_cast<int>(total_len)}))
        != Status::SUCCESS) {
      HOLOSCAN_LOG_ERROR("Failed to set packet length for packet {}", pkt_idx);
      free_all_packets_and_burst_tx(burst);
      return;
    }
  }

  // Record CUDA event to track completion
  cudaEventRecord(events_[cur_idx_], streams_[cur_idx_]);
  
  // Queue the burst for transmission
  out_q_.push(TxMsg{burst, events_[cur_idx_]});

  // Advance to next concurrent slot
  cur_idx_ = (cur_idx_ + 1) % num_concurrent;

  // Send any completed bursts
  while (!out_q_.empty()) {
    const auto& front = out_q_.front();
    if (cudaEventQuery(front.evt) == cudaSuccess) {
      // CUDA work complete, send to NIC
      if ((ret = send_tx_burst(front.msg)) != Status::SUCCESS) {
        HOLOSCAN_LOG_ERROR("Failed to send TX burst: {}", static_cast<int>(ret));
      } else {
        HOLOSCAN_LOG_INFO("! Sent {} packets ({} bytes)",
                           get_num_packets(front.msg), tensor_bytes);
      }
      out_q_.pop();
    } else {
      // Not ready yet, stop checking
      break;
    }
  }
}

}  // namespace holoscan::ops
