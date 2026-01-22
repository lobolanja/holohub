/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPU Direct Network TX/RX Roundtrip Integration Tests
 *
 * This test file contains two test fixtures:
 *
 * 1. TxRxRoundtripTest (Loopback Mode)
 *    - Tests software loopback using DPDK loopback interface
 *    - No hardware requirements
 *    - Run with: ./test_tx_rx_roundtrip --gtest_filter="TxRxRoundtripTest.*"
 *
 * 2. PhysicalNicRoundtripTest (Physical Hardware Mode)
 *    - Tests actual hardware data path using two physical NICs
 *    - Requires:
 *      * Two GPUDirect RDMA capable NICs (Mellanox ConnectX-6 or newer)
 *      * Physical cable connecting the two NICs
 *      * Environment variables:
 *        - TEST_TX_NIC_PCIE: PCIe address of TX NIC (e.g., "0005:03:00.0")
 *        - TEST_RX_NIC_PCIE: PCIe address of RX NIC (e.g., "0005:03:00.1")
 *        - TEST_RX_MAC: MAC address of RX NIC (e.g., "3c:6d:66:11:91:56")
 *    - Run with: ./test_tx_rx_roundtrip --gtest_filter="PhysicalNicRoundtripTest.*"
 *
 * Example test execution:
 *   # Set environment for physical NIC test
 *   export TEST_TX_NIC_PCIE="0005:03:00.0"
 *   export TEST_RX_NIC_PCIE="0005:03:00.1"
 *   export TEST_RX_MAC="3c:6d:66:11:91:56"
 *
 *   # Run all tests
 *   ./test_tx_rx_roundtrip
 *
 *   # Run only loopback tests
 *   ./test_tx_rx_roundtrip --gtest_filter="TxRxRoundtripTest.*"
 *
 *   # Run only physical NIC tests
 *   ./test_tx_rx_roundtrip --gtest_filter="PhysicalNicRoundtripTest.*"
 */

#include <gtest/gtest.h>
#include <gpu_direct_network_sender.h>
#include <gpu_direct_network_receiver.h>
#include <sender_config.h>
#include <receiver_config.h>
#include <gpu_direct_exceptions.h>
#include <advanced_network/common.h>
#include <advanced_network/manager.h>
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
SenderConfig create_loopback_sender_config() {
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
ReceiverConfig create_loopback_receiver_config() {
  ReceiverConfig config;
  config.interface_name = "loopback_ports";
  config.header_size = 64;
  config.max_packet_size = 1064;
  config.gpu_device = 0;
  return config;
}

/**
 * @brief Create sender configuration for physical NIC testing
 */
SenderConfig create_physical_sender_config() {
  const char* rx_mac = std::getenv("TEST_RX_MAC");
  if (!rx_mac) {
    throw std::runtime_error("TEST_RX_MAC environment variable not set");
  }
  
  SenderConfig config;
  config.interface_name = "tx_port";
  config.queue_id = 0;
  config.ip_src_addr = "192.168.10.10";
  config.ip_dst_addr = "192.168.10.11";
  config.eth_dst_addr = rx_mac;
  config.udp_src_port = 4096;
  config.udp_dst_port = 4096;
  config.header_size = 64;
  config.max_packet_size = 1064;
  return config;
}

/**
 * @brief Create receiver configuration for physical NIC testing
 */
ReceiverConfig create_physical_receiver_config() {
  ReceiverConfig config;
  config.interface_name = "rx_port";
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

#ifndef TEST_PHYSICAL_ONLY
/**
 * @brief Integration test fixture for TX/RX roundtrip (Loopback Mode)
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
    
    // Start DPDK worker threads (required for TX/RX to function)
    auto& mgr = holoscan::advanced_network::ManagerFactory::get_active_manager();
    mgr.run();
  }
  
  void TearDown() override { 
    // Shutdown Advanced Network manager to stop DPDK threads cleanly
    holoscan::advanced_network::shutdown();
    
    // Synchronize CUDA to ensure cleanup
    cudaDeviceSynchronize();
  }
};

/**
 * @brief Test TX/RX roundtrip with 10 samples, expecting at least 8 received
 */
TEST_F(TxRxRoundtripTest, SendReceive10SamplesExpect8) {
  // Create configurations
  auto sender_config = create_loopback_sender_config();
  auto receiver_config = create_loopback_receiver_config();
  
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
  
  // Allow time for final packets to be fully processed
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  // Verify statistics
  auto tx_stats = sender->get_stats();
  auto rx_stats = receiver->get_stats();
  
  // TX stats may report slightly fewer packets due to async transmission queuing
  EXPECT_GE(tx_stats.packets_sent, MIN_EXPECTED_PACKETS) 
    << "Expected at least " << MIN_EXPECTED_PACKETS << " packets sent, got " 
    << tx_stats.packets_sent;
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
#endif // TEST_PHYSICAL_ONLY

#ifndef TEST_LOOPBACK_ONLY
/**
 * @brief Integration test fixture for TX/RX roundtrip using physical NICs
 */
class PhysicalNicRoundtripTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Check for required environment variables
    const char* tx_pcie = std::getenv("TEST_TX_NIC_PCIE");
    const char* rx_pcie = std::getenv("TEST_RX_NIC_PCIE");
    const char* rx_mac = std::getenv("TEST_RX_MAC");
    
    ASSERT_TRUE(tx_pcie != nullptr) 
      << "TEST_TX_NIC_PCIE environment variable not set. "
      << "Example: export TEST_TX_NIC_PCIE=\"0005:03:00.0\"";
    ASSERT_TRUE(rx_pcie != nullptr) 
      << "TEST_RX_NIC_PCIE environment variable not set. "
      << "Example: export TEST_RX_NIC_PCIE=\"0005:03:00.1\"";
    ASSERT_TRUE(rx_mac != nullptr) 
      << "TEST_RX_MAC environment variable not set. "
      << "Example: export TEST_RX_MAC=\"3c:6d:66:11:91:56\"";
    
    // Initialize CUDA
    cudaError_t err = cudaSetDevice(0);
    ASSERT_EQ(err, cudaSuccess) << "Failed to set CUDA device: " << cudaGetErrorString(err);
    
    // Initialize Advanced Network manager with physical NIC configuration
    using namespace holoscan::advanced_network;
    
    NetworkConfig adv_net_config;
    
    // Common configuration
    adv_net_config.common_.version = 1;
    adv_net_config.common_.master_core_ = 3;
    adv_net_config.common_.manager_type = ManagerType::DPDK;
    adv_net_config.common_.loopback_ = LoopbackType::DISABLED;
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
    
    // TX Interface configuration
    InterfaceConfig tx_if;
    tx_if.name_ = "tx_port";
    tx_if.address_ = tx_pcie;
    
    // Disable flow isolation to allow promiscuous mode (like demo_ano_tx_rx.yaml)
    tx_if.rx_.flow_isolation_ = false;
    
    // TX queue (primary use)
    TxQueueConfig tx_q;
    tx_q.common_.id_ = 0;
    tx_q.common_.name_ = "tx_q_0";
    tx_q.common_.batch_size_ = 10240;
    tx_q.common_.cpu_core_ = "11";
    tx_q.common_.mrs_.push_back("Data_TX_GPU");
    tx_q.common_.offloads_.push_back("tx_eth_src");
    tx_if.tx_.queues_.push_back(tx_q);
    
    // Dummy RX queue (required by DPDK even if not used) - use different core than TX
    RxQueueConfig dummy_rx_q;
    dummy_rx_q.common_.id_ = 0;
    dummy_rx_q.common_.name_ = "dummy_rx_q_0";
    dummy_rx_q.common_.batch_size_ = 10240;
    dummy_rx_q.common_.cpu_core_ = "10";  // Different core from TX queue
    dummy_rx_q.timeout_us_ = 1000;
    dummy_rx_q.common_.mrs_.push_back("Data_TX_GPU");
    tx_if.rx_.queues_.push_back(dummy_rx_q);
    
    adv_net_config.ifs_.push_back(tx_if);
    
    // RX Interface configuration
    InterfaceConfig rx_if;
    rx_if.name_ = "rx_port";
    rx_if.address_ = rx_pcie;
    
    // Disable flow isolation to allow promiscuous mode (like demo_ano_tx_rx.yaml)
    rx_if.rx_.flow_isolation_ = false;
    
    // RX queue (primary use)
    RxQueueConfig rx_q;
    rx_q.common_.id_ = 0;
    rx_q.common_.name_ = "rx_q_0";
    rx_q.common_.batch_size_ = 10240;
    rx_q.common_.cpu_core_ = "9";
    rx_q.timeout_us_ = 1000;
    rx_q.common_.mrs_.push_back("Data_RX_GPU");
    rx_if.rx_.queues_.push_back(rx_q);
    
    // Dummy TX queue (required by DPDK even if not used)
    TxQueueConfig dummy_tx_q;
    dummy_tx_q.common_.id_ = 0;
    dummy_tx_q.common_.name_ = "dummy_tx_q_0";
    dummy_tx_q.common_.batch_size_ = 10240;
    dummy_tx_q.common_.cpu_core_ = "11";
    dummy_tx_q.common_.mrs_.push_back("Data_TX_GPU");
    dummy_tx_q.common_.offloads_.push_back("tx_eth_src");
    rx_if.tx_.queues_.push_back(dummy_tx_q);
    
    adv_net_config.ifs_.push_back(rx_if);
    
    // Initialize Advanced Network manager
    auto status = holoscan::advanced_network::adv_net_init(adv_net_config);
    ASSERT_EQ(status, holoscan::advanced_network::Status::SUCCESS) 
      << "Failed to initialize Advanced Network manager with physical NICs";
    
    // Note: run() is called automatically by Advanced Network manager during initialization
  }
  
  void TearDown() override { 
    // Shutdown Advanced Network manager to stop DPDK threads cleanly
    holoscan::advanced_network::shutdown();
    
    // Synchronize CUDA to ensure cleanup
    cudaDeviceSynchronize();
  }
};

/**
 * @brief Test TX/RX roundtrip with physical NICs: 10 samples, expecting at least 8 received
 */
TEST_F(PhysicalNicRoundtripTest, SendReceive10SamplesExpect8) {
  // Create configurations
  auto sender_config = create_physical_sender_config();
  auto receiver_config = create_physical_receiver_config();
  
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
  
  // Flush phase: ensure all TX bursts are sent to wire
  // The burst manager queues bursts and only sends them when CUDA events complete.
  // We need to poll periodically to allow event checking and burst transmission.
  for (int flush_iter = 0; flush_iter < 50; ++flush_iter) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // Calling is_ready() internally checks and may advance the CUDA slot tracker
    sender->is_ready();
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
  
  // Allow time for final packets to be fully processed
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  // Verify statistics
  auto tx_stats = sender->get_stats();
  auto rx_stats = receiver->get_stats();
  
  // TX stats may report slightly fewer packets due to async transmission queuing
  EXPECT_GE(tx_stats.packets_sent, MIN_EXPECTED_PACKETS) 
    << "Expected at least " << MIN_EXPECTED_PACKETS << " packets sent, got " 
    << tx_stats.packets_sent;
  EXPECT_GE(rx_stats.packets_received, MIN_EXPECTED_PACKETS) 
    << "Expected at least " << MIN_EXPECTED_PACKETS << " packets received, got " 
    << rx_stats.packets_received;
  
  // Verify we received at least MIN_EXPECTED_PACKETS unique samples
  EXPECT_GE(received_samples.size(), static_cast<size_t>(MIN_EXPECTED_PACKETS))
    << "Expected at least " << MIN_EXPECTED_PACKETS << " unique packets, got " 
    << received_samples.size();
  
  // Log statistics
  std::cout << "[Physical NIC] TX Statistics: packets=" << tx_stats.packets_sent 
            << ", bytes=" << tx_stats.bytes_transmitted 
            << ", dropped=" << tx_stats.frames_dropped << std::endl;
  std::cout << "[Physical NIC] RX Statistics: packets=" << rx_stats.packets_received 
            << ", bytes=" << rx_stats.bytes_received 
            << ", polls=" << rx_stats.polls_attempted 
            << ", empty_polls=" << rx_stats.empty_polls << std::endl;
  std::cout << "[Physical NIC] Unique received samples: " << received_samples.size() << std::endl;
}
#endif // TEST_LOOPBACK_ONLY

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
