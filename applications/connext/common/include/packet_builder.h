/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <cstdint>
#include <network_utils.h>

namespace holoscan::ops {

/**
 * @brief Packet construction utilities for network transmission
 * 
 * Provides functions to build complete packet headers for various protocols.
 * Thread-safe for concurrent calls.
 */
namespace PacketBuilder {

  /**
   * @brief Create a UDP/IPv4/Ethernet packet header
   * 
   * Constructs a complete packet header with Ethernet, IPv4, and UDP layers.
   * All checksums and source MAC are configured for NIC hardware offload.
   * 
   * @param max_packet_size Maximum packet size including all headers (bytes)
   * @param udp_src_port Source UDP port number (host byte order)
   * @param udp_dst_port Destination UDP port number (host byte order)
   * @param ip_src Source IP address (network byte order)
   * @param ip_dst Destination IP address (network byte order)
   * @param eth_dst Destination MAC address (6 bytes)
   * @param ip_ttl IP Time-To-Live value (default: 64)
   * @return Populated UDPIPV4Pkt structure ready for transmission
   * 
   * Thread-safe: Yes
   * 
   * Example:
   * @code
   * auto pkt = PacketBuilder::create_udp_ipv4_packet(
   *   9000, 5000, 4096,
   *   htonl(0xC0A80A0A),  // 192.168.10.10
   *   htonl(0xC0A80A0B),  // 192.168.10.11
   *   {0x3C, 0x6D, 0x66, 0x11, 0x91, 0x56}
   * );
   * @endcode
   */
  UDPIPV4Pkt create_udp_ipv4_packet(
    uint16_t max_packet_size,
    uint16_t udp_src_port,
    uint16_t udp_dst_port,
    uint32_t ip_src,
    uint32_t ip_dst,
    const std::array<uint8_t, 6>& eth_dst,
    uint8_t ip_ttl = 64
  );

}  // namespace PacketBuilder

}  // namespace holoscan::ops
