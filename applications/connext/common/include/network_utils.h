/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <array>
#include <cstdint>

namespace holoscan::ops {

/**
 * @brief Network configuration parameters grouped together
 */
struct NetworkConfig {
  std::string interface_name;
  std::string ip_src_addr;
  std::string ip_dst_addr;
  std::string eth_dst_addr;
  uint16_t udp_src_port;
  uint16_t udp_dst_port;
  uint16_t header_size;
  uint16_t max_packet_size;
  uint32_t batch_size;
  int header_data_split;  // 0 = GPU-only mode
};

/**
 * @brief Network utility functions for address parsing
 * 
 * All functions throw std::runtime_error on invalid input.
 * Thread-safe for concurrent calls.
 */
namespace NetworkUtils {
  // Parse MAC address string (format: "XX:XX:XX:XX:XX:XX") into byte array
  // Throws std::runtime_error if format is invalid
  void parse_mac_address(const std::string& mac_str, std::array<uint8_t, 6>& mac_bytes);
  
  // Parse IPv4 address string into network byte order uint32_t
  // Throws std::runtime_error if format is invalid
  uint32_t parse_ipv4_address(const std::string& ip_str);
  
  // Parse IPv4 address string into host byte order uint32_t
  // Throws std::runtime_error if format is invalid
  uint32_t parse_ipv4_address_host_order(const std::string& ip_str);
}  // namespace NetworkUtils

}  // namespace holoscan::ops
