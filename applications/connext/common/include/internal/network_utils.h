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
 * @brief Packet structure for UDP/IPv4 packet construction
 * 
 * Packed structure representing a complete UDP/IPv4/Ethernet packet header.
 * Used for manual packet header construction in network transmission.
 */
struct __attribute__((packed)) UDPIPV4Pkt {
  struct {
    uint8_t h_dest[6];
    uint8_t h_source[6];
    uint16_t h_proto;
  } eth;
  struct {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
  } ip;
  struct {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
  } udp;
} __attribute__((packed));

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
