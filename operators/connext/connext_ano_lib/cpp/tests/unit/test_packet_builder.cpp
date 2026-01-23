/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <arpa/inet.h>
#include "test_helpers.h"
#include "connext_ano_lib/internal/network_utils.h"

using namespace holoscan::ops;
using namespace connext_ano_lib::test;

// ============================================================================
// Packet Builder Tests
// ============================================================================

TEST(PacketBuilder, ValidInputs_CreatePacket_EthernetHeaderCorrect) {
  // Test data
  const std::array<uint8_t, 6> dst_mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  const uint16_t max_packet_size = 9000;
  const uint16_t udp_src_port = 5000;
  const uint16_t udp_dst_port = 5001;
  const uint32_t ip_src = htonl(0xC0A80A0A);  // 192.168.10.10
  const uint32_t ip_dst = htonl(0xC0A80A0B);  // 192.168.10.11
  
  // Create packet header
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    max_packet_size, udp_src_port, udp_dst_port,
    ip_src, ip_dst, dst_mac
  );
  
  // Verify Ethernet header
  EXPECT_EQ(0xaa, pkt.eth.h_dest[0]);
  EXPECT_EQ(0xbb, pkt.eth.h_dest[1]);
  EXPECT_EQ(0xcc, pkt.eth.h_dest[2]);
  EXPECT_EQ(0xdd, pkt.eth.h_dest[3]);
  EXPECT_EQ(0xee, pkt.eth.h_dest[4]);
  EXPECT_EQ(0xff, pkt.eth.h_dest[5]);
  
  // Source MAC should be all zeros (NIC will fill it via hardware offload)
  EXPECT_EQ(0x00, pkt.eth.h_source[0]);
  EXPECT_EQ(0x00, pkt.eth.h_source[1]);
  EXPECT_EQ(0x00, pkt.eth.h_source[2]);
  EXPECT_EQ(0x00, pkt.eth.h_source[3]);
  EXPECT_EQ(0x00, pkt.eth.h_source[4]);
  EXPECT_EQ(0x00, pkt.eth.h_source[5]);
  
  // EtherType should be 0x0800 (IPv4) in network byte order
  EXPECT_EQ(htons(0x0800), pkt.eth.h_proto);
}

TEST(PacketBuilder, ValidInputs_CreatePacket_Ipv4HeaderCorrect) {
  const std::array<uint8_t, 6> dst_mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  const uint16_t max_packet_size = 9000;
  const uint16_t udp_src_port = 5000;
  const uint16_t udp_dst_port = 5001;
  const uint32_t ip_src = htonl(0xC0A80A0A);  // 192.168.10.10
  const uint32_t ip_dst = htonl(0xC0A80A0B);  // 192.168.10.11
  
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    max_packet_size, udp_src_port, udp_dst_port,
    ip_src, ip_dst, dst_mac, 64
  );
  
  // Verify IPv4 header
  EXPECT_EQ(4, pkt.ip.version);  // IPv4
  EXPECT_EQ(5, pkt.ip.ihl);      // Header length: 5 * 4 = 20 bytes (no options)
  EXPECT_EQ(17, pkt.ip.protocol); // UDP protocol number
  EXPECT_EQ(64, pkt.ip.ttl);     // Time to live
  
  // Verify IP addresses (already in network byte order)
  EXPECT_EQ(ip_src, pkt.ip.saddr);
  EXPECT_EQ(ip_dst, pkt.ip.daddr);
}

TEST(PacketBuilder, ValidInputs_CreatePacket_UdpHeaderCorrect) {
  const std::array<uint8_t, 6> dst_mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  const uint16_t max_packet_size = 9000;
  const uint16_t udp_src_port = 5000;
  const uint16_t udp_dst_port = 5001;
  const uint32_t ip_src = htonl(0xC0A80A0A);
  const uint32_t ip_dst = htonl(0xC0A80A0B);
  
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    max_packet_size, udp_src_port, udp_dst_port,
    ip_src, ip_dst, dst_mac
  );
  
  // Verify UDP header (ports should be in network byte order)
  EXPECT_EQ(htons(udp_src_port), pkt.udp.source);
  EXPECT_EQ(htons(udp_dst_port), pkt.udp.dest);
}

TEST(PacketBuilder, ValidInputs_CreatePacket_ChecksumsZeroForOffload) {
  const std::array<uint8_t, 6> dst_mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  const uint16_t max_packet_size = 9000;
  const uint16_t udp_src_port = 5000;
  const uint16_t udp_dst_port = 5001;
  const uint32_t ip_src = htonl(0xC0A80A0A);
  const uint32_t ip_dst = htonl(0xC0A80A0B);
  
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    max_packet_size, udp_src_port, udp_dst_port,
    ip_src, ip_dst, dst_mac
  );
  
  // Checksums should be 0 for NIC hardware offload
  EXPECT_EQ(0, pkt.ip.check);   // IP checksum
  EXPECT_EQ(0, pkt.udp.check);  // UDP checksum
}

TEST(PacketBuilder, ValidInputs_CreatePacket_NetworkByteOrder) {
  const std::array<uint8_t, 6> dst_mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  const uint16_t max_packet_size = 9000;
  const uint16_t udp_src_port = 5000;
  const uint16_t udp_dst_port = 5001;
  const uint32_t ip_src = htonl(0xC0A80A0A);
  const uint32_t ip_dst = htonl(0xC0A80A0B);
  
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    max_packet_size, udp_src_port, udp_dst_port,
    ip_src, ip_dst, dst_mac
  );
  
  // Verify all multi-byte fields are in network byte order
  // EtherType
  uint16_t eth_proto_host = ntohs(pkt.eth.h_proto);
  EXPECT_EQ(0x0800, eth_proto_host);
  
  // UDP ports
  uint16_t src_port_host = ntohs(pkt.udp.source);
  uint16_t dst_port_host = ntohs(pkt.udp.dest);
  EXPECT_EQ(udp_src_port, src_port_host);
  EXPECT_EQ(udp_dst_port, dst_port_host);
  
  // IP addresses
  EXPECT_EQ(ip_src, pkt.ip.saddr);
  EXPECT_EQ(ip_dst, pkt.ip.daddr);
}

TEST(PacketBuilder, DifferentTtl_CreatePacket_TtlCorrect) {
  const std::array<uint8_t, 6> dst_mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  const uint16_t max_packet_size = 9000;
  const uint16_t udp_src_port = 5000;
  const uint16_t udp_dst_port = 5001;
  const uint32_t ip_src = htonl(0xC0A80A0A);
  const uint32_t ip_dst = htonl(0xC0A80A0B);
  
  // Test with custom TTL value
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    max_packet_size, udp_src_port, udp_dst_port,
    ip_src, ip_dst, dst_mac, 128
  );
  
  EXPECT_EQ(128, pkt.ip.ttl);
}

TEST(PacketBuilder, MinimalPacketSize_CreatePacket_HeaderSizeCorrect) {
  const std::array<uint8_t, 6> dst_mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  const uint16_t max_packet_size = 64;  // Minimal ethernet frame
  const uint16_t udp_src_port = 5000;
  const uint16_t udp_dst_port = 5001;
  const uint32_t ip_src = htonl(0xC0A80A0A);
  const uint32_t ip_dst = htonl(0xC0A80A0B);
  
  UDPIPV4Pkt pkt = PacketBuilder::create_udp_ipv4_packet(
    max_packet_size, udp_src_port, udp_dst_port,
    ip_src, ip_dst, dst_mac
  );
  
  // Verify packet was created successfully
  // Header size: 14 (Eth) + 20 (IP) + 8 (UDP) = 42 bytes minimum
  EXPECT_EQ(4, pkt.ip.version);
  EXPECT_EQ(5, pkt.ip.ihl);
}
