/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tensor_printer_op.h"
#include <cstring>

namespace holoscan::ops {

TensorPrinterOp::~TensorPrinterOp() {
  if (host_buffer_) {
    cudaFreeHost(host_buffer_);
  }
  if (cuda_stream_) {
    cudaStreamDestroy(cuda_stream_);
  }
}

void TensorPrinterOp::setup(OperatorSpec& spec) {
  spec.input<std::shared_ptr<holoscan::Tensor>>("tensor_in");
  
  spec.param(max_print_size_, "max_print_size", "Max Print Size",
             "Maximum bytes to print from tensor", static_cast<size_t>(256));
}

void TensorPrinterOp::initialize() {
  HOLOSCAN_LOG_INFO("TensorPrinterOp::initialize()");
  Operator::initialize();
  
  // Allocate host-pinned buffer for fast GPU->CPU copies
  host_buffer_size_ = max_print_size_.get();
  cudaError_t cuda_result = cudaMallocHost(&host_buffer_, host_buffer_size_);
  if (cuda_result != cudaSuccess) {
    throw std::runtime_error(
        fmt::format("Failed to allocate host buffer: {}", cudaGetErrorString(cuda_result)));
  }
  
  // Create CUDA stream
  cudaStreamCreate(&cuda_stream_);
  
  HOLOSCAN_LOG_INFO("TensorPrinterOp initialized with {} byte print buffer", host_buffer_size_);
}

void TensorPrinterOp::compute(InputContext& op_input, OutputContext& op_output,
                              ExecutionContext& context) {
  // Receive tensor
  auto maybe_tensor = op_input.receive<std::shared_ptr<holoscan::Tensor>>("tensor_in");
  if (!maybe_tensor) {
    HOLOSCAN_LOG_WARN("No tensor received");
    return;
  }
  
  auto tensor = maybe_tensor.value();
  
  // Validate tensor is on GPU
  DLDevice dev = tensor->device();
  if (dev.device_type != kDLCUDA) {
    std::string device_type_str = (dev.device_type == kDLCPU) ? "CPU" : "Unknown";
    HOLOSCAN_LOG_ERROR("Expected tensor on CUDA device, got device type: {}", device_type_str);
    return;
  }
  
  // Get tensor info
  void* gpu_data = tensor->data();
  size_t tensor_bytes = tensor->nbytes();
  
  // Copy from GPU to CPU (limited by max_print_size)
  size_t copy_size = std::min(tensor_bytes, host_buffer_size_);
  cudaMemcpyAsync(host_buffer_, gpu_data, copy_size,
                  cudaMemcpyDeviceToHost, cuda_stream_);
  cudaStreamSynchronize(cuda_stream_);
  
  // Print as string (ensure null-termination)
  char* str_buffer = static_cast<char*>(host_buffer_);
  str_buffer[copy_size - 1] = '\0';  // Safety null-terminator
  
  packets_received_++;
  
  HOLOSCAN_LOG_INFO("📥 Received packet #{}: '{}' ({} bytes from GPU)",
                    packets_received_, str_buffer, tensor_bytes);
  
  // Also print to stdout for visibility
  std::cout << "📥 Received packet #" << packets_received_ 
            << ": '" << str_buffer << "' (" << tensor_bytes << " bytes from GPU)" 
            << std::endl;
}

}  // namespace holoscan::ops
