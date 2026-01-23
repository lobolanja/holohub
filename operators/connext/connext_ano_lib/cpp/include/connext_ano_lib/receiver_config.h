/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <cstdint>
#include <connext_ano_lib/gpu_direct_exceptions.h>

namespace holoscan::ops {

/**
 * @brief Configuration for GPU Direct network receiver
 * 
 * Plain struct with validation, no Holoscan dependencies.
 * All fields must be set before calling validate().
 */
struct ReceiverConfig {
  std::string interface_name;  ///< NIC interface name from advanced_network config
  uint16_t header_size;        ///< Header size to skip (Eth+IP+UDP, minimum 42 bytes)
  uint16_t max_packet_size;    ///< Maximum expected packet size including headers
  int gpu_device;              ///< GPU device ID for CUDA operations
  
  /**
   * @brief Validate configuration parameters
   * 
   * Checks that all required fields are set and values are within valid ranges.
   * 
   * @throws InvalidConfigException if any parameter is invalid
   */
  void validate() const {
    if (interface_name.empty()) {
      throw InvalidConfigException("interface_name cannot be empty");
    }
    
    if (header_size < 42) {
      throw InvalidConfigException(
        "header_size must be at least 42 bytes (Ethernet + IP + UDP minimum)");
    }
    
    if (max_packet_size <= header_size) {
      throw InvalidConfigException(
        "max_packet_size must be greater than header_size");
    }
    
    if (gpu_device < 0) {
      throw InvalidConfigException("gpu_device must be non-negative");
    }
  }
};

}  // namespace holoscan::ops
