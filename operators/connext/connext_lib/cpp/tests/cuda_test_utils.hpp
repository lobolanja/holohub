/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cuda_runtime.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

namespace connext_lib {
namespace test {

/**
 * @brief Check CUDA error and throw exception if failed
 */
inline void checkCudaError(cudaError_t err, const char* msg) {
  if (err != cudaSuccess) {
    throw std::runtime_error(std::string(msg) + ": " + cudaGetErrorString(err));
  }
}

/**
 * @brief RAII wrapper for CUDA device memory
 */
class CudaMemoryGuard {
 public:
  explicit CudaMemoryGuard(size_t size) : size_(size) {
    cudaError_t err = cudaMalloc(&ptr_, size);
    checkCudaError(err, "cudaMalloc failed");
  }
  
  ~CudaMemoryGuard() {
    if (ptr_) {
      cudaFree(ptr_);
    }
  }
  
  // Delete copy
  CudaMemoryGuard(const CudaMemoryGuard&) = delete;
  CudaMemoryGuard& operator=(const CudaMemoryGuard&) = delete;
  
  // Allow move
  CudaMemoryGuard(CudaMemoryGuard&& other) noexcept : ptr_(other.ptr_), size_(other.size_) {
    other.ptr_ = nullptr;
    other.size_ = 0;
  }
  
  CudaMemoryGuard& operator=(CudaMemoryGuard&& other) noexcept {
    if (this != &other) {
      if (ptr_) cudaFree(ptr_);
      ptr_ = other.ptr_;
      size_ = other.size_;
      other.ptr_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }
  
  void* get() const { return ptr_; }
  size_t size() const { return size_; }
  
 private:
  void* ptr_ = nullptr;
  size_t size_ = 0;
};

/**
 * @brief Copy data from CPU to GPU memory
 * @param cpu_data Pointer to CPU data
 * @param size Size in bytes
 * @return CudaMemoryGuard managing the GPU memory
 */
inline CudaMemoryGuard copyToGpu(const void* cpu_data, size_t size) {
  CudaMemoryGuard gpu_mem(size);
  cudaError_t err = cudaMemcpy(gpu_mem.get(), cpu_data, size, cudaMemcpyHostToDevice);
  checkCudaError(err, "cudaMemcpy Host->Device failed");
  return gpu_mem;
}

/**
 * @brief Copy data from GPU to CPU memory
 * @param gpu_data Pointer to GPU data
 * @param size Size in bytes
 * @return Vector containing the copied data
 */
inline std::vector<uint8_t> copyFromGpu(const void* gpu_data, size_t size) {
  std::vector<uint8_t> cpu_data(size);
  cudaError_t err = cudaMemcpy(cpu_data.data(), gpu_data, size, cudaMemcpyDeviceToHost);
  checkCudaError(err, "cudaMemcpy Device->Host failed");
  return cpu_data;
}

/**
 * @brief Copy string from GPU to CPU
 * @param gpu_data Pointer to GPU data
 * @param size Size in bytes
 * @return String containing the copied data
 */
inline std::string copyStringFromGpu(const void* gpu_data, size_t size) {
  auto cpu_data = copyFromGpu(gpu_data, size);
  return std::string(cpu_data.begin(), cpu_data.end());
}

}  // namespace test
}  // namespace connext_lib
