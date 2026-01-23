/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <connext_ano_lib/internal/cuda_resource_manager.h>
#include <connext_ano_lib/gpu_direct_exceptions.h>

namespace holoscan::ops {

CudaResourceManager::CudaResourceManager(int num_concurrent)
    : num_concurrent_(num_concurrent > 0 ? num_concurrent : DEFAULT_CONCURRENT) {
  streams_.reserve(num_concurrent_);
  events_.reserve(num_concurrent_);
  
  for (int i = 0; i < num_concurrent_; i++) {
    streams_.emplace_back();
    events_.emplace_back();
  }
}

bool CudaResourceManager::is_ready() const {
  return events_[cur_idx_].query() == cudaSuccess;
}

cudaStream_t CudaResourceManager::get_stream() const {
  return streams_[cur_idx_].get();
}

cudaEvent_t CudaResourceManager::get_event() const {
  return events_[cur_idx_].get();
}

void CudaResourceManager::record_and_advance() {
  events_[cur_idx_].record(streams_[cur_idx_].get());
  cur_idx_ = (cur_idx_ + 1) % num_concurrent_;
}

void* CudaResourceManager::allocate_buffer(size_t size) {
  void* ptr = nullptr;
  cudaError_t err = cudaMalloc(&ptr, size);
  if (err != cudaSuccess) {
    throw CudaInitException(
        "cudaMalloc failed for " + std::to_string(size) + " bytes: " +
        cudaGetErrorString(err));
  }
  return ptr;
}

void CudaResourceManager::free_buffer(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  
  cudaError_t err = cudaFree(ptr);
  if (err != cudaSuccess) {
    throw CudaInitException(
        "cudaFree failed: " + std::string(cudaGetErrorString(err)));
  }
}

void CudaResourceManager::async_copy_device_to_device(void* dst, void* src, size_t size) {
  cudaError_t err = cudaMemcpyAsync(dst, src, size, 
                                     cudaMemcpyDeviceToDevice, get_stream());
  if (err != cudaSuccess) {
    throw CudaInitException(
        "cudaMemcpyAsync failed: " + std::string(cudaGetErrorString(err)));
  }
}

//
// CudaBuffer implementation
//

CudaBuffer::CudaBuffer(size_t size) : size_(size) {
  cudaError_t err = cudaMalloc(&ptr_, size);
  if (err != cudaSuccess) {
    throw CudaInitException(
        "cudaMalloc failed for " + std::to_string(size) + " bytes: " +
        cudaGetErrorString(err));
  }
}

CudaBuffer::~CudaBuffer() {
  if (ptr_) {
    cudaFree(ptr_);
  }
}

CudaBuffer::CudaBuffer(CudaBuffer&& other) noexcept
    : ptr_(other.ptr_), size_(other.size_) {
  other.ptr_ = nullptr;
  other.size_ = 0;
}

CudaBuffer& CudaBuffer::operator=(CudaBuffer&& other) noexcept {
  if (this != &other) {
    if (ptr_) cudaFree(ptr_);
    ptr_ = other.ptr_;
    size_ = other.size_;
    other.ptr_ = nullptr;
    other.size_ = 0;
  }
  return *this;
}

}  // namespace holoscan::ops
