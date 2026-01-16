/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <holoscan/holoscan.hpp>
#include <cuda_runtime.h>

namespace holoscan::ops {

/**
 * @brief Operator that generates Holoscan Tensors with test data on GPU
 * 
 * Generates tensors containing a string like "hello world #N" where N is the frame number.
 * The data is created on CPU and copied to GPU memory.
 */
class TensorGeneratorOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TensorGeneratorOp)

  TensorGeneratorOp() = default;
  ~TensorGeneratorOp() override;

  void setup(OperatorSpec& spec) override;
  void initialize() override;
  void compute(InputContext& op_input, OutputContext& op_output,
               ExecutionContext& context) override;

 private:
  Parameter<std::string> base_message_;
  Parameter<size_t> tensor_size_;
  Parameter<int> gpu_device_;
  Parameter<std::string> recess_period_;
  
  int frame_count_ = 0;
  cudaStream_t cuda_stream_ = nullptr;
};

}  // namespace holoscan::ops
