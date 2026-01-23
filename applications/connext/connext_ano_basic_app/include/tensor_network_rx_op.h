/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <holoscan/holoscan.hpp>
#include <connext_ano_lib/gpu_direct_network_receiver.h>

namespace holoscan::ops {

/**
 * @brief Operator to receive Holoscan Tensor data over network using GPUDirect
 * 
 * Receives packets from the network using GPU Direct facade and reconstructs them 
 * into Holoscan Tensors on GPU. All DPDK and CUDA complexity is encapsulated
 * in the IGpuDirectNetworkReceiver facade.
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
  Parameter<uint16_t> max_packet_size_;
  Parameter<uint16_t> header_size_;
  Parameter<int> gpu_device_;
  Parameter<uint64_t> max_count_;  // 0 = unlimited, N = stop after N packets

  // GPU Direct network receiver facade
  std::unique_ptr<IGpuDirectNetworkReceiver> receiver_;

  // Statistics
  uint64_t packets_received_ = 0;
};

}  // namespace holoscan::ops
