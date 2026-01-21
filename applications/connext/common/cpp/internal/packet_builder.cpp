/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../../include/internal/packet_builder.h"
#include <arpa/inet.h>
#include <cstring>

namespace holoscan::ops {

namespace PacketBuilder {

// Network protocol constants
namespace {
  constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
  constexpr uint8_t IP_HEADER_LENGTH_WORDS = 5;  // 20 bytes / 4
}

UDPIPV4Pkt create_udp_ipv4_packet(
    uint16_t max_packet_size,
    uint16_t udp_src_port,
    uint16_t udp_dst_port,
    uint32_t ip_src,
    uint32_t ip_dst,
    const std::array<uint8_t, 6>& eth_dst,
    uint8_t ip_ttl) {
  
  UDPIPV4Pkt pkt = {};
  
  // Ethernet header
  std::memcpy(pkt.eth.h_dest, eth_dst.data(), sizeof(pkt.eth.h_dest));
  // Source MAC will be set by NIC offload
  std::memset(pkt.eth.h_source, 0, sizeof(pkt.eth.h_source));
  pkt.eth.h_proto = htons(ETHERTYPE_IPV4);

  // IP header  
  uint16_t ip_len = max_packet_size - sizeof(pkt.eth);
  
  pkt.ip.version = 4;
  pkt.ip.ihl = IP_HEADER_LENGTH_WORDS;
  pkt.ip.tos = 0;
  pkt.ip.tot_len = htons(ip_len);
  pkt.ip.id = 0;
  pkt.ip.frag_off = 0;
  pkt.ip.ttl = ip_ttl;
  pkt.ip.protocol = IPPROTO_UDP;
  pkt.ip.check = 0;  // Offloaded to NIC
  pkt.ip.saddr = ip_src;
  pkt.ip.daddr = ip_dst;

  // UDP header
  pkt.udp.source = htons(udp_src_port);
  pkt.udp.dest = htons(udp_dst_port);
  pkt.udp.len = htons(ip_len - sizeof(pkt.ip));
  pkt.udp.check = 0;  // Offloaded to NIC
  
  return pkt;
}

}  // namespace PacketBuilder

}  // namespace holoscan::ops
