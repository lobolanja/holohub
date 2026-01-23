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
 * Provides a slot-based system for managing multiple concurrent CUDA operations.
 * Each slot contains a stream and event pair, allowing overlap of GPU work with
 * CPU operations. The manager cycles through slots, checking for completion
 * before reusing resources.
 * 
 * Typical usage pattern:
 * 1. Call is_ready() to check if current slot is available
 * 2. Use get_stream() for async CUDA operations
 * 3. Call record_and_advance() to mark work complete and move to next slot
 * 
 * Thread-safety: Not thread-safe. Each instance should be used by a single thread only.
 */
class CudaResourceManager {
 public:
  /**
   * @brief Construct a CUDA resource manager
   * 
   * Creates the specified number of stream/event pairs for concurrent operations.
   * 
   * @param num_concurrent Number of concurrent slots (default: 4)
   */
  explicit CudaResourceManager(int num_concurrent = 4);
  ~CudaResourceManager() = default;
  
  /**
   * @brief Check if the current resource slot is ready for new work
   * 
   * Queries the CUDA event associated with the current slot to determine
   * if previous operations have completed. Should be called before starting
   * new work on the current slot.
   * 
   * @return true if slot is ready (event completed), false otherwise
   */
  bool is_ready() const;
  
  /**
   * @brief Get the current CUDA stream for async operations
   * 
   * Returns the stream for the current slot. Use this stream for all
   * async CUDA operations (memcpy, kernels, etc.) to enable proper
   * event-based synchronization.
   * 
   * @return CUDA stream handle for current slot
   */
  cudaStream_t get_stream() const;
  
  /**
   * @brief Get the current CUDA event for synchronization
   * 
   * Returns the event for the current slot. This event will be recorded
   * when record_and_advance() is called.
   * 
   * @return CUDA event handle for current slot
   */
  cudaEvent_t get_event() const;
  
  /**
   * @brief Record an event on the current stream and advance to next slot
   * 
   * Records the current slot's event on its stream to mark work completion,
   * then advances the internal index to the next slot (wrapping around).
   * This enables round-robin slot usage for overlapping GPU operations.
   * 
   * Call this after submitting all async work for the current slot.
   */
  void record_and_advance();
  
  /**
   * @brief Get the current slot index
   * 
   * @return Current slot index (0 to num_concurrent-1)
   */
  int current_index() const { return cur_idx_; }
  
  /**
   * @brief Allocate GPU buffer
   * 
   * Allocates device memory using cudaMalloc. Caller is responsible for
   * freeing the buffer with free_buffer() or cudaFree().
   * 
   * @param size Size in bytes to allocate
   * @return Pointer to allocated GPU memory
   * @throws CudaInitException if cudaMalloc fails
   */
  void* allocate_buffer(size_t size);
  
  /**
   * @brief Free GPU buffer
   * 
   * Frees device memory allocated with allocate_buffer() or cudaMalloc.
   * Safe to call with nullptr (no-op).
   * 
   * @param ptr Pointer to GPU memory to free (can be nullptr)
   * @throws CudaInitException if cudaFree fails
   */
  void free_buffer(void* ptr);
  
  /**
   * @brief Async device-to-device copy using internal stream
   * 
   * Performs GPU-to-GPU memory copy on the current slot's stream.
   * This is an async operation - use record_and_advance() and event
   * queries to synchronize.
   * 
   * @param dst Destination GPU pointer
   * @param src Source GPU pointer
   * @param size Number of bytes to copy
   * @throws CudaInitException if cudaMemcpyAsync fails
   */
  void async_copy_device_to_device(void* dst, void* src, size_t size);
  
 private:
  static constexpr int DEFAULT_CONCURRENT = 4;
  int num_concurrent_;
  std::vector<CudaStream> streams_;
  std::vector<CudaEvent> events_;
  int cur_idx_ = 0;
};

/**
 * @brief RAII wrapper for CUDA device memory buffer
 * 
 * Automatically manages GPU memory allocation and deallocation.
 * Useful for persistent GPU buffers (e.g., packet headers).
 * 
 * Thread-safety: Not thread-safe. Each instance should be used by a single thread only.
 */
class CudaBuffer {
 public:
  /**
   * @brief Allocate GPU memory buffer
   * 
   * @param size Size in bytes to allocate
   * @throws CudaInitException if cudaMalloc fails
   */
  explicit CudaBuffer(size_t size);
  
  /**
   * @brief Free GPU memory
   */
  ~CudaBuffer();
  
  // Delete copy constructor and assignment
  CudaBuffer(const CudaBuffer&) = delete;
  CudaBuffer& operator=(const CudaBuffer&) = delete;
  
  // Allow move
  CudaBuffer(CudaBuffer&& other) noexcept;
  CudaBuffer& operator=(CudaBuffer&& other) noexcept;
  
  /**
   * @brief Get raw GPU pointer
   * @return Pointer to GPU memory
   */
  void* data() const { return ptr_; }
  
  /**
   * @brief Get buffer size
   * @return Size in bytes
   */
  size_t size() const { return size_; }
  
 private:
  void* ptr_ = nullptr;
  size_t size_ = 0;
};

}  // namespace holoscan::ops
