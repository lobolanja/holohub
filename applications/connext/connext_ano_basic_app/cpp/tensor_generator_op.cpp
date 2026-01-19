/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/tensor_generator_op.h"
#include <cstring>

namespace holoscan::ops {

TensorGeneratorOp::~TensorGeneratorOp() {
  if (cuda_stream_) {
    cudaStreamDestroy(cuda_stream_);
  }
}

void TensorGeneratorOp::setup(OperatorSpec& spec) {
  spec.output<std::shared_ptr<holoscan::Tensor>>("tensor_out");
  
  spec.param(base_message_, "base_message", "Base Message",
             "Base message to send in tensor", std::string("hello world"));
  spec.param(tensor_size_, "tensor_size", "Tensor Size",
             "Size of tensor in bytes", static_cast<size_t>(1024));
  spec.param(gpu_device_, "gpu_device", "GPU Device",
             "GPU device ID", 0);
  spec.param(recess_period_, "recess_period", "Recess Period",
             "Recess period for PeriodicCondition", std::string("1s"));
}

void TensorGeneratorOp::initialize() {
  HOLOSCAN_LOG_INFO("TensorGeneratorOp::initialize()");
  Operator::initialize();
  
  // Set GPU device
  cudaSetDevice(gpu_device_.get());
  
  // Create CUDA stream for async operations
  cudaStreamCreate(&cuda_stream_);
  
  HOLOSCAN_LOG_INFO("TensorGeneratorOp initialized on GPU {}", gpu_device_.get());
}

void TensorGeneratorOp::compute(InputContext& op_input, OutputContext& op_output,
                                ExecutionContext& context) { 
  // Create message with frame number
  std::string message = base_message_.get() + " #" + std::to_string(frame_count_++);
  
  // Ensure message fits in tensor size
  size_t message_size = std::min(message.size() + 1, tensor_size_.get());  // +1 for null terminator
  
  // Allocate GPU memory for tensor using shared_ptr with custom deleter
  void* gpu_data = nullptr;
  cudaError_t cuda_result = cudaMalloc(&gpu_data, tensor_size_.get());
  if (cuda_result != cudaSuccess) {
    HOLOSCAN_LOG_ERROR("Failed to allocate GPU memory: {}", cudaGetErrorString(cuda_result));
    return;
  }
  
  // Create shared_ptr with custom deleter for GPU memory management
  std::shared_ptr<void*> gpu_data_ptr(new void*(gpu_data), [](void** ptr) {
    if (ptr != nullptr) {
      if (*ptr != nullptr) {
        cudaFree(*ptr);
        *ptr = nullptr;
      }
      delete ptr;
      ptr = nullptr;
    }
  });
  
  // Zero out the memory first
  cudaMemsetAsync(gpu_data, 0, tensor_size_.get(), cuda_stream_);
  
  // Copy message to GPU
  cudaMemcpyAsync(gpu_data, message.c_str(), message_size,
                  cudaMemcpyHostToDevice, cuda_stream_);
  
  // Wait for copy to complete
  cudaStreamSynchronize(cuda_stream_);
  
  // Debug: Verify data was copied correctly
  std::vector<uint8_t> debug_data(std::min(32UL, tensor_size_.get()));
  cudaMemcpy(debug_data.data(), gpu_data, debug_data.size(), cudaMemcpyDeviceToHost);
  HOLOSCAN_LOG_INFO("Generated data (first {} bytes): {}", debug_data.size(), 
                    fmt::join(debug_data, " "));
  
  // Create DLPack tensor descriptor using DLManagedTensorContext
  auto dl_context = std::make_shared<DLManagedTensorContext>();
  dl_context->memory_ref = gpu_data_ptr;
  
  // Setup shape (must live as long as the tensor)
  dl_context->dl_shape = {static_cast<int64_t>(tensor_size_.get())};
  
  // Setup DLTensor
  dl_context->tensor.dl_tensor.data = gpu_data;
  dl_context->tensor.dl_tensor.device = DLDevice{kDLCUDA, gpu_device_.get()};
  dl_context->tensor.dl_tensor.ndim = 1;
  dl_context->tensor.dl_tensor.dtype = DLDataType{kDLUInt, 8, 1};  // uint8
  dl_context->tensor.dl_tensor.shape = dl_context->dl_shape.data();
  dl_context->tensor.dl_tensor.strides = nullptr;
  dl_context->tensor.dl_tensor.byte_offset = 0;
  
  // Create Holoscan Tensor from DLManagedTensorContext
  auto output_tensor = std::make_shared<holoscan::Tensor>(dl_context);
  
  HOLOSCAN_LOG_DEBUG("Generated tensor with message: '{}' ({} bytes on GPU)",
                     message, tensor_size_.get());
  
  // Emit tensor
  op_output.emit(output_tensor, "tensor_out");
}

}  // namespace holoscan::ops
