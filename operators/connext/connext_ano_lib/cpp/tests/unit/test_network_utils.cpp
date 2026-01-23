/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <arpa/inet.h>
#include "test_helpers.h"
#include "connext_ano_lib/internal/network_utils.h"

using namespace holoscan::ops::NetworkUtils;
using namespace connext_ano_lib::test;

// ============================================================================
// MAC Address Parsing Tests
// ============================================================================

TEST(NetworkUtilsParseMac, ValidFormat_Parse_ReturnsCorrectBytes) {
  std::array<uint8_t, 6> mac_bytes;
  parse_mac_address(kTestMacValid, mac_bytes);
  
  EXPECT_EQ(0xaa, mac_bytes[0]);
  EXPECT_EQ(0xbb, mac_bytes[1]);
  EXPECT_EQ(0xcc, mac_bytes[2]);
  EXPECT_EQ(0xdd, mac_bytes[3]);
  EXPECT_EQ(0xee, mac_bytes[4]);
  EXPECT_EQ(0xff, mac_bytes[5]);
}

TEST(NetworkUtilsParseMac, AllZeros_Parse_ReturnsCorrectBytes) {
  std::array<uint8_t, 6> mac_bytes;
  parse_mac_address("00:00:00:00:00:00", mac_bytes);
  
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(0x00, mac_bytes[i]);
  }
}

TEST(NetworkUtilsParseMac, MixedCase_Parse_ReturnsCorrectBytes) {
  std::array<uint8_t, 6> mac_bytes;
  parse_mac_address("Aa:Bb:Cc:Dd:Ee:Ff", mac_bytes);
  
  EXPECT_EQ(0xaa, mac_bytes[0]);
  EXPECT_EQ(0xbb, mac_bytes[1]);
  EXPECT_EQ(0xcc, mac_bytes[2]);
  EXPECT_EQ(0xdd, mac_bytes[3]);
  EXPECT_EQ(0xee, mac_bytes[4]);
  EXPECT_EQ(0xff, mac_bytes[5]);
}

TEST(NetworkUtilsParseMac, MissingColon_Parse_ThrowsRuntimeError) {
  std::array<uint8_t, 6> mac_bytes;
  EXPECT_THROW(parse_mac_address("aa:bb:cc:dd:eeff", mac_bytes), std::runtime_error);
}

TEST(NetworkUtilsParseMac, InvalidHex_Parse_ThrowsRuntimeError) {
  std::array<uint8_t, 6> mac_bytes;
  EXPECT_THROW(parse_mac_address("zz:bb:cc:dd:ee:ff", mac_bytes), std::runtime_error);
}

TEST(NetworkUtilsParseMac, TooShort_Parse_ThrowsRuntimeError) {
  std::array<uint8_t, 6> mac_bytes;
  EXPECT_THROW(parse_mac_address("aa:bb:cc:dd:ee", mac_bytes), std::runtime_error);
}

TEST(NetworkUtilsParseMac, TooLong_Parse_ParsesFirst6Bytes) {
  // Note: sscanf-based implementation parses first 6 hex values and ignores trailing content.
  // This is acceptable since config validation ensures well-formed MAC addresses.
  std::array<uint8_t, 6> mac_bytes;
  parse_mac_address("aa:bb:cc:dd:ee:ff:00", mac_bytes);
  EXPECT_EQ(mac_bytes[0], 0xaa);
  EXPECT_EQ(mac_bytes[1], 0xbb);
  EXPECT_EQ(mac_bytes[2], 0xcc);
  EXPECT_EQ(mac_bytes[3], 0xdd);
  EXPECT_EQ(mac_bytes[4], 0xee);
  EXPECT_EQ(mac_bytes[5], 0xff);
}

// ============================================================================
// IPv4 Address Parsing Tests (Network Byte Order)
// ============================================================================

TEST(NetworkUtilsParseIpv4, ValidIp_Parse_ReturnsNetworkByteOrder) {
  uint32_t ip = parse_ipv4_address(kTestIpValid);
  
  // 192.168.10.10 in network byte order
  // Verify by comparing with inet_addr which returns network byte order
  uint32_t expected = inet_addr(kTestIpValid);
  EXPECT_EQ(expected, ip);
}

TEST(NetworkUtilsParseIpv4, AllZeros_Parse_ReturnsZero) {
  uint32_t ip = parse_ipv4_address("0.0.0.0");
  EXPECT_EQ(0u, ip);
}

TEST(NetworkUtilsParseIpv4, AllOnes_Parse_ReturnsCorrectValue) {
  uint32_t ip = parse_ipv4_address("255.255.255.255");
  uint32_t expected = inet_addr("255.255.255.255");
  EXPECT_EQ(expected, ip);
}

TEST(NetworkUtilsParseIpv4, OctetAbove255_Parse_ThrowsRuntimeError) {
  EXPECT_THROW(parse_ipv4_address("192.168.10.256"), std::runtime_error);
}

TEST(NetworkUtilsParseIpv4, NegativeOctet_Parse_ThrowsRuntimeError) {
  EXPECT_THROW(parse_ipv4_address("192.168.-1.10"), std::runtime_error);
}

TEST(NetworkUtilsParseIpv4, TooFewOctets_Parse_ThrowsRuntimeError) {
  EXPECT_THROW(parse_ipv4_address("192.168.10"), std::runtime_error);
}

TEST(NetworkUtilsParseIpv4, TooManyOctets_Parse_ThrowsRuntimeError) {
  EXPECT_THROW(parse_ipv4_address("192.168.10.10.1"), std::runtime_error);
}

TEST(NetworkUtilsParseIpv4, EmptyString_Parse_ThrowsRuntimeError) {
  EXPECT_THROW(parse_ipv4_address(""), std::runtime_error);
}

// ============================================================================
// IPv4 Address Parsing Tests (Host Byte Order)
// ============================================================================

TEST(NetworkUtilsParseIpv4HostOrder, ValidIp_Parse_ReturnsHostByteOrder) {
  uint32_t ip_host = parse_ipv4_address_host_order(kTestIpValid);
  uint32_t ip_network = parse_ipv4_address(kTestIpValid);
  
  // Verify host and network byte orders are related by htonl
  EXPECT_EQ(htonl(ip_host), ip_network);
}

TEST(NetworkUtilsParseIpv4HostOrder, ByteOrderComparison_MatchesNetworkOrder) {
  const char* test_ip = "192.168.10.11";
  uint32_t ip_host = parse_ipv4_address_host_order(test_ip);
  uint32_t ip_network = parse_ipv4_address(test_ip);
  
  // Convert host to network and verify they match
  EXPECT_EQ(htonl(ip_host), ip_network);
  
  // Convert network to host and verify they match
  EXPECT_EQ(ntohl(ip_network), ip_host);
}

TEST(NetworkUtilsParseIpv4HostOrder, OctetExtraction_MatchesExpected) {
  // 192.168.10.10 in host order should have octets accessible
  uint32_t ip_host = parse_ipv4_address_host_order("192.168.10.10");
  
  // In host byte order (little endian on most platforms)
  uint8_t octet4 = (ip_host >> 24) & 0xFF;  // 192
  uint8_t octet3 = (ip_host >> 16) & 0xFF;  // 168
  uint8_t octet2 = (ip_host >> 8) & 0xFF;   // 10
  uint8_t octet1 = ip_host & 0xFF;          // 10
  
  EXPECT_EQ(192, octet4);
  EXPECT_EQ(168, octet3);
  EXPECT_EQ(10, octet2);
  EXPECT_EQ(10, octet1);
}
