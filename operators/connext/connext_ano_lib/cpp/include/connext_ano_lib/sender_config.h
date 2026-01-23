/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <connext_ano_lib/gpu_direct_exceptions.h>

namespace holoscan::ops {

/**
 * @brief Configuration for GPU Direct network sender
 * 
 * Plain struct with public fields for network transmission configuration.
 * All fields must be populated before calling validate().
 * 
 * GPU-only mode: No header-data split, entire packet assembled on GPU.
 * Minimum header_size is 42 bytes (Ethernet 14 + IPv4 20 + UDP 8).
 */
struct SenderConfig {
  // Network interface
  std::string interface_name;  ///< Interface name from advanced_network config
  uint16_t queue_id = 0;       ///< TX queue ID (must match advanced_network config)
  
  // Network addresses
  std::string ip_src_addr;     ///< Source IPv4 address (e.g., "192.168.10.10")
  std::string ip_dst_addr;     ///< Destination IPv4 address
  std::string eth_dst_addr;    ///< Destination MAC address (e.g., "3c:6d:66:11:91:56")
  
  // Network ports
  uint16_t udp_src_port = 5000;  ///< Source UDP port
  uint16_t udp_dst_port = 5001;  ///< Destination UDP port
  
  // Packet sizing
  uint16_t header_size = 64;       ///< Header size in bytes (minimum 42 for Eth+IP+UDP)
  uint16_t max_packet_size = 9000; ///< Maximum packet size including headers
  
  /**
   * @brief Validate configuration parameters
   * 
   * Checks all fields for validity and consistency. Call this before
   * passing config to IGpuDirectNetworkSender::create().
   * 
   * @throws InvalidConfigException if any field is invalid
   * 
   * Validation rules:
   * - interface_name must not be empty
   * - IP addresses must be valid IPv4 format
   * - MAC address must be valid 6-byte format
   * - header_size must be >= 42 bytes (Eth+IP+UDP minimum)
   * - max_packet_size must be > header_size
   * - udp_src_port and udp_dst_port must be valid (1-65535)
   */
  void validate() const {
    if (interface_name.empty()) {
      throw InvalidConfigException("interface_name cannot be empty");
    }
    
    if (ip_src_addr.empty()) {
      throw InvalidConfigException("ip_src_addr cannot be empty");
    }
    
    if (ip_dst_addr.empty()) {
      throw InvalidConfigException("ip_dst_addr cannot be empty");
    }
    
    if (eth_dst_addr.empty()) {
      throw InvalidConfigException("eth_dst_addr cannot be empty");
    }
    
    if (udp_src_port == 0) {
      throw InvalidConfigException("udp_src_port must be between 1 and 65535");
    }
    
    if (udp_dst_port == 0) {
      throw InvalidConfigException("udp_dst_port must be between 1 and 65535");
    }
    
    constexpr uint16_t MIN_HEADER_SIZE = 42;  // Eth(14) + IP(20) + UDP(8)
    if (header_size < MIN_HEADER_SIZE) {
      throw InvalidConfigException(
          "header_size must be at least " + std::to_string(MIN_HEADER_SIZE) + 
          " bytes (Ethernet + IPv4 + UDP), got " + std::to_string(header_size));
    }
    
    if (max_packet_size <= header_size) {
      throw InvalidConfigException(
          "max_packet_size (" + std::to_string(max_packet_size) + 
          ") must be greater than header_size (" + std::to_string(header_size) + ")");
    }
  }
};

}  // namespace holoscan::ops
