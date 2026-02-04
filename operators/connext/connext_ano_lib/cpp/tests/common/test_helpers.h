/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONNEXT_ANO_LIB_TEST_HELPERS_H
#define CONNEXT_ANO_LIB_TEST_HELPERS_H

#include <gtest/gtest.h>
#include <cuda_runtime.h>
#include <cstdlib>
#include <cstring>

#include "connext_ano_lib/sender_config.h"
#include "connext_ano_lib/receiver_config.h"

namespace connext_ano_lib {
namespace test {

// Common test data constants (extracted when used in 3+ test files)
inline constexpr const char* kTestMacValid = "aa:bb:cc:dd:ee:ff";
inline constexpr const char* kTestIpValid = "192.168.10.10";
inline constexpr const char* kTestIpValidAlt = "192.168.10.11";

// CUDA assertion macro for component tests
#define ASSERT_CUDA_SUCCESS(call) \
  do { \
    cudaError_t err = (call); \
    ASSERT_EQ(cudaSuccess, err) << "CUDA error: " << cudaGetErrorString(err); \
  } while (0)

// Helper function to create a valid SenderConfig for testing
inline holoscan::ops::SenderConfig CreateValidSenderConfig() {
  holoscan::ops::SenderConfig config;
  config.interface_name = "eth0";
  config.ip_src_addr = "192.168.10.10";
  config.ip_dst_addr = "192.168.10.11";
  config.eth_dst_addr = "aa:bb:cc:dd:ee:ff";
  config.udp_src_port = 5000;
  config.udp_dst_port = 5001;
  config.header_size = 64;
  config.max_packet_size = 9000;
  return config;
}

// Helper function to create a valid ReceiverConfig for testing
inline holoscan::ops::ReceiverConfig CreateValidReceiverConfig() {
  holoscan::ops::ReceiverConfig config;
  config.interface_name = "eth0";
  config.header_size = 64;
  config.max_packet_size = 9000;
  config.gpu_device = 0;
  config.queue_id = 0;
  return config;
}

// CUDA test fixture for component tests
// Handles GPU device selection via TEST_GPU_DEVICE environment variable
class CudaTestFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    // Select GPU device (default to 0, override with TEST_GPU_DEVICE env var)
    const char* device_env = std::getenv("TEST_GPU_DEVICE");
    gpu_device_ = device_env ? std::atoi(device_env) : 0;
    
    ASSERT_CUDA_SUCCESS(cudaSetDevice(gpu_device_));
    
    // Query device properties for diagnostic purposes
    cudaDeviceProp prop;
    ASSERT_CUDA_SUCCESS(cudaGetDeviceProperties(&prop, gpu_device_));
    device_name_ = prop.name;
  }

  void TearDown() override {
    // Ensure all pending CUDA operations complete
    ASSERT_CUDA_SUCCESS(cudaDeviceSynchronize());
  }

  int gpu_device_ = 0;
  std::string device_name_;
};

}  // namespace test
}  // namespace connext_ano_lib

#endif  // CONNEXT_ANO_LIB_TEST_HELPERS_H
