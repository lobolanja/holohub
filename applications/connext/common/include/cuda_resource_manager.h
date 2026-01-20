/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cuda_runtime.h>
#include <vector>

namespace holoscan::ops {

/**
 * @brief RAII wrapper for CUDA stream
 * 
 * Thread-safety: Not thread-safe. Each instance should be used by a single thread only.
 */
class CudaStream {
 public:
  CudaStream() { cudaStreamCreate(&stream_); }
  ~CudaStream() { 
    if (stream_) {
      cudaStreamDestroy(stream_);
    }
  }
  
  // Delete copy constructor and assignment
  CudaStream(const CudaStream&) = delete;
  CudaStream& operator=(const CudaStream&) = delete;
  
  // Allow move
  CudaStream(CudaStream&& other) noexcept : stream_(other.stream_) {
    other.stream_ = nullptr;
  }
  CudaStream& operator=(CudaStream&& other) noexcept {
    if (this != &other) {
      if (stream_) cudaStreamDestroy(stream_);
      stream_ = other.stream_;
      other.stream_ = nullptr;
    }
    return *this;
  }
  
  operator cudaStream_t() const { return stream_; }
  cudaStream_t get() const { return stream_; }
  
 private:
  cudaStream_t stream_ = nullptr;
};

/**
 * @brief RAII wrapper for CUDA event
 * 
 * Thread-safety: Not thread-safe. Each instance should be used by a single thread only.
 */
class CudaEvent {
 public:
  CudaEvent() { cudaEventCreate(&event_); }
  ~CudaEvent() {
    if (event_) {
      cudaEventDestroy(event_);
    }
  }
  
  // Delete copy constructor and assignment
  CudaEvent(const CudaEvent&) = delete;
  CudaEvent& operator=(const CudaEvent&) = delete;
  
  // Allow move
  CudaEvent(CudaEvent&& other) noexcept : event_(other.event_) {
    other.event_ = nullptr;
  }
  CudaEvent& operator=(CudaEvent&& other) noexcept {
    if (this != &other) {
      if (event_) cudaEventDestroy(event_);
      event_ = other.event_;
      other.event_ = nullptr;
    }
    return *this;
  }
  
  operator cudaEvent_t() const { return event_; }
  cudaEvent_t get() const { return event_; }
  
  cudaError_t query() const { return cudaEventQuery(event_); }
  cudaError_t record(cudaStream_t stream) { return cudaEventRecord(event_, stream); }
  
 private:
  cudaEvent_t event_ = nullptr;
};

/**
 * @brief Manages CUDA resources (streams and events) for async operations
 * 
 * Thread-safety: Not thread-safe. Each instance should be used by a single thread only.
 */
class CudaResourceManager {
 public:
  explicit CudaResourceManager(int num_concurrent = 4);
  ~CudaResourceManager() = default;
  
  // Check if the current resource slot is ready for new work
  bool is_ready() const;
  
  // Get the current CUDA stream
  cudaStream_t get_stream() const;
  
  // Get the current CUDA event
  cudaEvent_t get_event() const;
  
  // Record an event on the current stream and advance to next slot
  void record_and_advance();
  
  // Get the current slot index
  int current_index() const { return cur_idx_; }
  
 private:
  static constexpr int DEFAULT_CONCURRENT = 4;
  int num_concurrent_;
  std::vector<CudaStream> streams_;
  std::vector<CudaEvent> events_;
  int cur_idx_ = 0;
};

}  // namespace holoscan::ops
