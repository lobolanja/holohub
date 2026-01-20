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

using namespace holoscan::advanced_network;

namespace holoscan::ops {

/**
 * @brief Operator to transmit Holoscan Tensor data over network using GPUDirect
 * 
 * Receives Holoscan Tensors containing GPU data and transmits them to a remote host
 * using the Advanced Network library with DPDK backend and GPU-only mode.
 * The tensor data is sent directly from GPU memory to NIC via GPUDirect.
 */
class TensorNetworkTxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TensorNetworkTxOp)

  TensorNetworkTxOp() = default;
  ~TensorNetworkTxOp() override;

  void setup(OperatorSpec& spec) override;
  void initialize() override;
  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override;

 private:
  // Configuration parameters
  Parameter<std::string> interface_name_;
  Parameter<std::string> ip_src_addr_;
  Parameter<std::string> ip_dst_addr_;
  Parameter<std::string> eth_dst_addr_;
  Parameter<uint16_t> udp_src_port_;
  Parameter<uint16_t> udp_dst_port_;
  Parameter<uint16_t> header_size_;
  Parameter<uint16_t> max_packet_size_;
  Parameter<uint32_t> batch_size_;
  Parameter<int> hds_;  // Header-data split (0 = GPU-only mode)

  // Parsed network configuration
  NetworkConfig config_;

  // Network state
  int port_id_ = -1;
  uint16_t queue_id_ = 0;
  uint32_t ip_src_;
  uint32_t ip_dst_;
  char eth_dst_[6];

  // CUDA resource management
  CudaResourceManager cuda_manager_;

  // GPU header buffer for GPU-only mode
  void* gds_header_ = nullptr;
  
  // Packet structure for manual header construction
  struct __attribute__((packed)) UDPIPV4Pkt {
    struct {
      uint8_t h_dest[6];
      uint8_t h_source[6];
      uint16_t h_proto;
    } eth;
    struct {
      uint8_t ihl : 4;
      uint8_t version : 4;
      uint8_t tos;
      uint16_t tot_len;
      uint16_t id;
      uint16_t frag_off;
      uint8_t ttl;
      uint8_t protocol;
      uint16_t check;
      uint32_t saddr;
      uint32_t daddr;
    } ip;
    struct {
      uint16_t source;
      uint16_t dest;
      uint16_t len;
      uint16_t check;
    } udp;
  } __attribute__((packed));

  UDPIPV4Pkt pkt_;

  // Queue to track in-flight transmissions
  struct TxMsg {
    BurstParams* msg;
    cudaEvent_t evt;  // Store event for later query
  };
  std::queue<TxMsg> out_q_;

  // Helper functions for GPU-only mode
  void populate_packet_headers(const NetworkConfig& config);
  void parse_network_config();
  
  // Refactored compute() helper methods
  bool is_ready_for_transmission();
  std::optional<std::shared_ptr<holoscan::Tensor>> receive_and_validate_tensor(InputContext& op_input);
  void log_tensor_debug_info(const std::shared_ptr<holoscan::Tensor>& tensor);
  size_t validate_and_adjust_tensor_size(size_t tensor_bytes);
  bool prepare_tx_burst(BurstParams*& burst, int num_packets);
  bool populate_packet_data(BurstParams* burst, void* tensor_data, size_t tensor_bytes, int num_packets);
  void enqueue_transmission(BurstParams* burst);
  void process_pending_transmissions(size_t last_tensor_bytes);
};

}  // namespace holoscan::ops
