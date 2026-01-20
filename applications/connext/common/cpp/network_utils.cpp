/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/network_utils.h"
#include <arpa/inet.h>
#include <stdexcept>

namespace holoscan::ops {

namespace {
  constexpr int MAC_ADDRESS_COMPONENTS = 6;
}

namespace NetworkUtils {

void parse_mac_address(const std::string& mac_str, std::array<uint8_t, 6>& mac_bytes) {
  unsigned int values[MAC_ADDRESS_COMPONENTS];
  if (sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) != MAC_ADDRESS_COMPONENTS) {
    throw std::runtime_error("Invalid MAC address format: " + mac_str);
  }
  for (int i = 0; i < MAC_ADDRESS_COMPONENTS; i++) {
    mac_bytes[i] = static_cast<uint8_t>(values[i]);
  }
}

uint32_t parse_ipv4_address(const std::string& ip_str) {
  uint32_t ip_addr;
  if (inet_pton(AF_INET, ip_str.c_str(), &ip_addr) != 1) {
    throw std::runtime_error("Invalid IPv4 address format: " + ip_str);
  }
  return ip_addr;  // Network byte order
}

uint32_t parse_ipv4_address_host_order(const std::string& ip_str) {
  return ntohl(parse_ipv4_address(ip_str));
}

}  // namespace NetworkUtils

}  // namespace holoscan::ops
