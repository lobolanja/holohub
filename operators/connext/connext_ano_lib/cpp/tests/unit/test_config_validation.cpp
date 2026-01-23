/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include "test_helpers.h"
#include "connext_ano_lib/sender_config.h"
#include "connext_ano_lib/receiver_config.h"
#include "connext_ano_lib/gpu_direct_exceptions.h"

using namespace holoscan::ops;
using namespace connext_ano_lib::test;
using InvalidConfigException = holoscan::ops::InvalidConfigException;

// ============================================================================
// SenderConfig Validation Tests
// ============================================================================

TEST(SenderConfigValidation, ValidConfig_Validate_NoThrow) {
  SenderConfig config = CreateValidSenderConfig();
  EXPECT_NO_THROW(config.validate());
}

TEST(SenderConfigValidation, EmptyInterface_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.interface_name = "";
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, EmptySrcIp_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.ip_src_addr = "";
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, EmptyDstIp_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.ip_dst_addr = "";
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, EmptyDstMac_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.eth_dst_addr = "";
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, ZeroSrcPort_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.udp_src_port = 0;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, ZeroDstPort_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.udp_dst_port = 0;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, PortAbove65535_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.udp_src_port = 65536;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, HeaderSizeBelow42_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.header_size = 41;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(SenderConfigValidation, HeaderSize42_Validate_NoThrow) {
  SenderConfig config = CreateValidSenderConfig();
  config.header_size = 42;
  EXPECT_NO_THROW(config.validate());
}

TEST(SenderConfigValidation, MaxPacketSizeLessThanHeaderSize_Validate_ThrowsInvalidConfigException) {
  SenderConfig config = CreateValidSenderConfig();
  config.header_size = 100;
  config.max_packet_size = 99;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

// ============================================================================
// ReceiverConfig Validation Tests
// ============================================================================

TEST(ReceiverConfigValidation, ValidConfig_Validate_NoThrow) {
  ReceiverConfig config = CreateValidReceiverConfig();
  EXPECT_NO_THROW(config.validate());
}

TEST(ReceiverConfigValidation, EmptyInterface_Validate_ThrowsInvalidConfigException) {
  ReceiverConfig config = CreateValidReceiverConfig();
  config.interface_name = "";
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(ReceiverConfigValidation, HeaderSizeBelow42_Validate_ThrowsInvalidConfigException) {
  ReceiverConfig config = CreateValidReceiverConfig();
  config.header_size = 41;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(ReceiverConfigValidation, HeaderSize42_Validate_NoThrow) {
  ReceiverConfig config = CreateValidReceiverConfig();
  config.header_size = 42;
  EXPECT_NO_THROW(config.validate());
}

TEST(ReceiverConfigValidation, MaxPacketSizeLessThanHeaderSize_Validate_ThrowsInvalidConfigException) {
  ReceiverConfig config = CreateValidReceiverConfig();
  config.header_size = 100;
  config.max_packet_size = 99;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(ReceiverConfigValidation, NegativeGpuDevice_Validate_ThrowsInvalidConfigException) {
  ReceiverConfig config = CreateValidReceiverConfig();
  config.gpu_device = -1;
  EXPECT_THROW(config.validate(), InvalidConfigException);
}

TEST(ReceiverConfigValidation, GpuDeviceZero_Validate_NoThrow) {
  ReceiverConfig config = CreateValidReceiverConfig();
  config.gpu_device = 0;
  EXPECT_NO_THROW(config.validate());
}
