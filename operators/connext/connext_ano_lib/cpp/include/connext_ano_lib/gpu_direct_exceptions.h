/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdexcept>
#include <string>

namespace holoscan::ops {

/**
 * @brief Base exception for GPU Direct network sender errors
 */
class GpuDirectException : public std::runtime_error {
 public:
  explicit GpuDirectException(const std::string& message)
      : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when configuration is invalid
 * 
 * Examples:
 * - Invalid IP/MAC address format
 * - header_size < 42 bytes (minimum for Eth+IP+UDP)
 * - max_packet_size <= header_size
 * - Empty interface name
 */
class InvalidConfigException : public GpuDirectException {
 public:
  explicit InvalidConfigException(const std::string& message)
      : GpuDirectException("Invalid configuration: " + message) {}
};

/**
 * @brief Exception thrown during network initialization
 * 
 * Examples:
 * - Interface name not found in advanced_network config
 * - Port ID resolution failure
 * - DPDK port not available
 */
class NetworkInitException : public GpuDirectException {
 public:
  explicit NetworkInitException(const std::string& message)
      : GpuDirectException("Network initialization failed: " + message) {}
};

/**
 * @brief Exception thrown during CUDA initialization or operations
 * 
 * Examples:
 * - cudaMalloc failure (out of memory)
 * - cudaMemcpy failure
 * - Invalid CUDA device
 */
class CudaInitException : public GpuDirectException {
 public:
  explicit CudaInitException(const std::string& message)
      : GpuDirectException("CUDA initialization failed: " + message) {}
};

/**
 * @brief Exception thrown when sender is not ready for transmission
 * 
 * Thrown by send() if is_ready() returns false, indicating previous
 * CUDA batch is still in flight. Includes batch index for debugging.
 * 
 * Usage: Caller should check is_ready() before calling send() to avoid
 * this exception. If thrown despite pre-check, log and skip frame.
 */
class NotReadyException : public GpuDirectException {
 public:
  explicit NotReadyException(int batch_index)
      : GpuDirectException("Sender not ready, batch " + std::to_string(batch_index) + 
                          " still in flight"),
        batch_index_(batch_index) {}
  
  int batch_index() const { return batch_index_; }
  
 private:
  int batch_index_;
};

}  // namespace holoscan::ops
