/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <holoscan/holoscan.hpp>
#include <cuda_runtime.h>

namespace holoscan::ops {

/**
 * @brief Operator that receives and prints Holoscan Tensor contents
 * 
 * Receives tensors from GPU memory, copies to CPU, and prints the string content
 * for debugging purposes.
 */
class TensorPrinterOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TensorPrinterOp)

  TensorPrinterOp() = default;
  ~TensorPrinterOp() override;

  void setup(OperatorSpec& spec) override;
  void initialize() override;
  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override;

 private:
  Parameter<size_t> max_print_size_;
  
  cudaStream_t cuda_stream_ = nullptr;
  void* host_buffer_ = nullptr;
  size_t host_buffer_size_ = 0;
  int packets_received_ = 0;
};

}  // namespace holoscan::ops
