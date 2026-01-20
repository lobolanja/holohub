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

using namespace holoscan::advanced_network;

namespace holoscan::ops {

/**
 * @brief Operator to receive Holoscan Tensor data over network using GPUDirect
 * 
 * Receives packets from the network using Advanced Network library with DPDK backend
 * in GPU-only mode, and reconstructs them into Holoscan Tensors on GPU.
 */
class TensorNetworkRxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TensorNetworkRxOp)

  TensorNetworkRxOp() = default;
  ~TensorNetworkRxOp() override;

  void setup(OperatorSpec& spec) override;
  void initialize() override;
  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override;

 private:
  // Configuration parameters
  Parameter<std::string> interface_name_;
  Parameter<int> hds_;
  Parameter<uint32_t> batch_size_;
  Parameter<uint16_t> max_packet_size_;
  Parameter<uint16_t> header_size_;
  Parameter<int> gpu_device_;
  Parameter<uint64_t> max_count_;  // 0 = unlimited, N = stop after N packets

  // Network state
  int port_id_ = -1;

  // CUDA resource management
  CudaResourceManager cuda_manager_;

  // Statistics
  uint64_t packets_received_ = 0;
  uint64_t bytes_received_ = 0;

  // Queue to track in-flight processing
  struct RxBatch {
    BurstParams* burst;
    cudaEvent_t evt;
  };
  std::queue<RxBatch> batch_q_;

  // Helper functions
  void free_processed_packets();
};

}  // namespace holoscan::ops
