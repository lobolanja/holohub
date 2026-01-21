/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <gpu_direct_network_sender.h>
#include <gpu_direct_network_receiver.h>
#include <sender_config.h>
#include <receiver_config.h>
#include <gpu_direct_exceptions.h>
#include <advanced_network/common.h>
#include <cuda_runtime.h>
#include <yaml-cpp/yaml.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

using namespace holoscan::ops;

namespace {

constexpr int NUM_SAMPLES = 10;
constexpr int SAMPLE_INTERVAL_MS = 100;
constexpr int MIN_EXPECTED_PACKETS = 8;
constexpr size_t PAYLOAD_SIZE = 1000;
constexpr char TEST_PAYLOAD_PATTERN[] = "test_roundtrip_";

/**
 * @brief RAII wrapper for CUDA device memory
 */
class CudaMemoryGuard {
 public:
  explicit CudaMemoryGuard(size_t size) : size_(size) {
    cudaError_t err = cudaMalloc(&ptr_, size);
    if (err != cudaSuccess) {
      throw std::runtime_error(std::string("cudaMalloc failed: ") + cudaGetErrorString(err));
    }
  }
  
  ~CudaMemoryGuard() {
    if (ptr_) {
      cudaFree(ptr_);
    }
  }
  
  // Delete copy
  CudaMemoryGuard(const CudaMemoryGuard&) = delete;
  CudaMemoryGuard& operator=(const CudaMemoryGuard&) = delete;
  
  void* get() const { return ptr_; }
  size_t size() const { return size_; }
  
 private:
  void* ptr_ = nullptr;
  size_t size_ = 0;
};

/**
 * @brief Create sender configuration for loopback testing
 */
SenderConfig create_sender_config() {
  SenderConfig config;
  config.interface_name = "loopback_ports";
  config.queue_id = 0;
  config.ip_src_addr = "192.168.10.10";
  config.ip_dst_addr = "192.168.10.11";
  config.eth_dst_addr = "3c:6d:66:11:91:56";
  config.udp_src_port = 4096;
  config.udp_dst_port = 4096;
  config.header_size = 64;
  config.max_packet_size = 1064;
  return config;
}

/**
 * @brief Create receiver configuration for loopback testing
 */
ReceiverConfig create_receiver_config() {
  ReceiverConfig config;
  config.interface_name = "loopback_ports";
  config.header_size = 64;
  config.max_packet_size = 1064;
  config.gpu_device = 0;
  return config;
}

/**
 * @brief Populate GPU buffer with test pattern
 */
void populate_test_payload(void* gpu_ptr, size_t size, int sample_index) {
  std::vector<uint8_t> host_buffer(size);
  
  // Create pattern: "test_roundtrip_<index>_<repeated_data>"
  std::string pattern = std::string(TEST_PAYLOAD_PATTERN) + std::to_string(sample_index) + "_";
  size_t pattern_len = pattern.size();
  
  for (size_t i = 0; i < size; ++i) {
    host_buffer[i] = pattern[i % pattern_len];
  }
  
  cudaError_t err = cudaMemcpy(gpu_ptr, host_buffer.data(), size, cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    throw std::runtime_error(std::string("cudaMemcpy H2D failed: ") + cudaGetErrorString(err));
  }
}

/**
 * @brief Verify received payload matches expected pattern
 */
bool verify_payload(void* gpu_ptr, size_t size, int expected_sample_index) {
  std::vector<uint8_t> host_buffer(size);
  
  cudaError_t err = cudaMemcpy(host_buffer.data(), gpu_ptr, size, cudaMemcpyDeviceToHost);
  if (err != cudaSuccess) {
    return false;
  }
  
  // Check if pattern matches any sample index (packets may arrive out of order)
  std::string pattern = std::string(TEST_PAYLOAD_PATTERN) + std::to_string(expected_sample_index) + "_";
  size_t pattern_len = pattern.size();
  
  for (size_t i = 0; i < std::min(size, pattern_len); ++i) {
    if (host_buffer[i] != pattern[i % pattern_len]) {
      return false;
    }
  }
  
  return true;
}

} // anonymous namespace

/**
 * @brief Integration test fixture for TX/RX roundtrip
 */
class TxRxRoundtripTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize CUDA
    cudaError_t err = cudaSetDevice(0);
    ASSERT_EQ(err, cudaSuccess) << "Failed to set CUDA device: " << cudaGetErrorString(err);
    
    // Initialize Advanced Network manager with hardcoded config
    using namespace holoscan::advanced_network;
    
    NetworkConfig adv_net_config;
    
    // Common configuration
    adv_net_config.common_.version = 1;
    adv_net_config.common_.master_core_ = 3;
    adv_net_config.common_.dir = Direction::TX_RX;
    adv_net_config.common_.manager_type = ManagerType::DPDK;
    adv_net_config.common_.loopback_ = LoopbackType::LOOPBACK_TYPE_SW;
    adv_net_config.debug_ = 0;
    adv_net_config.tx_meta_buffers_ = 256;
    adv_net_config.rx_meta_buffers_ = 256;
    adv_net_config.log_level_ = LogLevel::Level::INFO;
    
    // Memory regions
    MemoryRegionConfig tx_mr;
    tx_mr.name_ = "Data_TX_GPU";
    tx_mr.kind_ = MemoryKind::DEVICE;
    tx_mr.affinity_ = 0;
    tx_mr.buf_size_ = 1064;
    tx_mr.num_bufs_ = 51200;
    tx_mr.owned_ = true;
    adv_net_config.mrs_["Data_TX_GPU"] = tx_mr;
    
    MemoryRegionConfig rx_mr;
    rx_mr.name_ = "Data_RX_GPU";
    rx_mr.kind_ = MemoryKind::DEVICE;
    rx_mr.affinity_ = 0;
    rx_mr.buf_size_ = 1064;
    rx_mr.num_bufs_ = 51200;
    rx_mr.owned_ = true;
    adv_net_config.mrs_["Data_RX_GPU"] = rx_mr;
    
    // Interface configuration
    InterfaceConfig loopback_if;
    loopback_if.name_ = "loopback_ports";
    loopback_if.address_ = "loopback";
    loopback_if.port_id_ = 0;
    
    // TX queue
    TxQueueConfig tx_q;
    tx_q.common_.id_ = 0;
    tx_q.common_.name_ = "tx_q_0";
    tx_q.common_.batch_size_ = 10240;
    tx_q.common_.cpu_core_ = "11";
    tx_q.common_.mrs_.push_back("Data_TX_GPU");
    tx_q.common_.offloads_.push_back("tx_eth_src");
    loopback_if.tx_.queues_.push_back(tx_q);
    
    // RX queue
    RxQueueConfig rx_q;
    rx_q.common_.id_ = 0;
    rx_q.common_.name_ = "rq_q_0";
    rx_q.common_.batch_size_ = 10240;
    rx_q.common_.cpu_core_ = "9";
    rx_q.timeout_us_ = 1000;
    rx_q.common_.mrs_.push_back("Data_RX_GPU");
    loopback_if.rx_.queues_.push_back(rx_q);
    
    adv_net_config.ifs_.push_back(loopback_if);
    
    // Initialize Advanced Network manager
    auto status = holoscan::advanced_network::adv_net_init(adv_net_config);
    ASSERT_EQ(status, holoscan::advanced_network::Status::SUCCESS) 
      << "Failed to initialize Advanced Network manager";
  }
  
  void TearDown() override {
    // Synchronize CUDA to ensure cleanup
    cudaDeviceSynchronize();
    
    // Note: Advanced Network manager shutdown happens automatically
    // No explicit shutdown call needed
  }
};

/**
 * @brief Test TX/RX roundtrip with 10 samples, expecting at least 8 received
 */
TEST_F(TxRxRoundtripTest, SendReceive10SamplesExpect8) {
  // Create configurations
  auto sender_config = create_sender_config();
  auto receiver_config = create_receiver_config();
  
  // Validate configurations
  ASSERT_NO_THROW(sender_config.validate());
  ASSERT_NO_THROW(receiver_config.validate());
  
  // Create sender and receiver facades
  std::unique_ptr<IGpuDirectNetworkSender> sender;
  std::unique_ptr<IGpuDirectNetworkReceiver> receiver;
  
  ASSERT_NO_THROW(sender = IGpuDirectNetworkSender::create(sender_config));
  ASSERT_NO_THROW(receiver = IGpuDirectNetworkReceiver::create(receiver_config));
  
  ASSERT_NE(sender, nullptr);
  ASSERT_NE(receiver, nullptr);
  
  // Allocate GPU buffer for TX
  CudaMemoryGuard tx_buffer(PAYLOAD_SIZE);
  
  // Track received packets
  std::vector<int> received_samples;
  int polls_attempted = 0;
  const int MAX_POLLS = NUM_SAMPLES * 20; // Allow extra polls for packet arrival
  
  // TX/RX loop: send 10 samples with 100ms intervals, poll for received packets
  for (int sample_idx = 0; sample_idx < NUM_SAMPLES; ++sample_idx) {
    // Prepare payload
    ASSERT_NO_THROW(populate_test_payload(tx_buffer.get(), PAYLOAD_SIZE, sample_idx));
    
    // Wait for sender readiness
    int wait_count = 0;
    while (!sender->is_ready() && wait_count < 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      wait_count++;
    }
    ASSERT_TRUE(sender->is_ready()) << "Sender not ready after waiting";
    
    // Send packet
    ASSERT_NO_THROW(sender->send(tx_buffer.get(), PAYLOAD_SIZE));
    
    // Poll for received packets (multiple times per sample)
    for (int poll = 0; poll < 2 && polls_attempted < MAX_POLLS; ++poll, ++polls_attempted) {
      auto received = receiver->receive();
      
      if (received.has_value()) {
        auto& data = received.value();
        ASSERT_EQ(data.payload_bytes, PAYLOAD_SIZE) << "Received payload size mismatch";
        
        // Verify payload (check against all sent samples since order not guaranteed)
        bool matched = false;
        for (int sent_idx = 0; sent_idx < sample_idx + 1; ++sent_idx) {
          if (verify_payload(data.gpu_payload, data.payload_bytes, sent_idx)) {
            received_samples.push_back(sent_idx);
            matched = true;
            break;
          }
        }
        
        EXPECT_TRUE(matched) << "Received payload doesn't match any sent pattern";
        
        // Free received buffer
        ASSERT_NO_THROW(receiver->free_received_data(data.gpu_payload));
      }
    }
    
    // Wait 100ms before next sample
    std::this_thread::sleep_for(std::chrono::milliseconds(SAMPLE_INTERVAL_MS));
  }
  
  // Final polling phase: collect remaining packets
  for (int final_poll = 0; final_poll < 20 && polls_attempted < MAX_POLLS; ++final_poll, ++polls_attempted) {
    auto received = receiver->receive();
    
    if (received.has_value()) {
      auto& data = received.value();
      ASSERT_EQ(data.payload_bytes, PAYLOAD_SIZE);
      
      bool matched = false;
      for (int sent_idx = 0; sent_idx < NUM_SAMPLES; ++sent_idx) {
        if (verify_payload(data.gpu_payload, data.payload_bytes, sent_idx)) {
          received_samples.push_back(sent_idx);
          matched = true;
          break;
        }
      }
      
      EXPECT_TRUE(matched);
      ASSERT_NO_THROW(receiver->free_received_data(data.gpu_payload));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  
  // Verify statistics
  auto tx_stats = sender->get_stats();
  auto rx_stats = receiver->get_stats();
  
  EXPECT_EQ(tx_stats.packets_sent, NUM_SAMPLES) << "Expected " << NUM_SAMPLES << " packets sent";
  EXPECT_GE(rx_stats.packets_received, MIN_EXPECTED_PACKETS) 
    << "Expected at least " << MIN_EXPECTED_PACKETS << " packets received, got " 
    << rx_stats.packets_received;
  
  // Verify we received at least MIN_EXPECTED_PACKETS unique samples
  EXPECT_GE(received_samples.size(), static_cast<size_t>(MIN_EXPECTED_PACKETS))
    << "Expected at least " << MIN_EXPECTED_PACKETS << " unique packets, got " 
    << received_samples.size();
  
  // Log statistics
  std::cout << "TX Statistics: packets=" << tx_stats.packets_sent 
            << ", bytes=" << tx_stats.bytes_transmitted 
            << ", dropped=" << tx_stats.frames_dropped << std::endl;
  std::cout << "RX Statistics: packets=" << rx_stats.packets_received 
            << ", bytes=" << rx_stats.bytes_received 
            << ", polls=" << rx_stats.polls_attempted 
            << ", empty_polls=" << rx_stats.empty_polls << std::endl;
  std::cout << "Unique received samples: " << received_samples.size() << std::endl;
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
