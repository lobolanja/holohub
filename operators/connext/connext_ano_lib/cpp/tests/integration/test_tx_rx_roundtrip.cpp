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
#include <connext_ano_lib/gpu_direct_network_sender.h>
#include <connext_ano_lib/gpu_direct_network_receiver.h>
#include <connext_ano_lib/sender_config.h>
#include <connext_ano_lib/receiver_config.h>
#include <connext_ano_lib/gpu_direct_exceptions.h>
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
  // Get network parameters from environment variables with defaults
  const char* tx_ip = std::getenv("TEST_TX_IP");
  const char* rx_ip = std::getenv("TEST_RX_IP");
  const char* eth_dst = std::getenv("TEST_ETH_DST_MAC");
  
  SenderConfig config;
  config.interface_name = "loopback_ports";
  config.queue_id = 0;
  config.ip_src_addr = tx_ip ? tx_ip : "192.168.10.10";
  config.ip_dst_addr = rx_ip ? rx_ip : "192.168.10.11";
  config.eth_dst_addr = eth_dst ? eth_dst : "3c:6d:66:11:91:56";
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
  // Get network parameters from environment variables
  const char* tx_ip = std::getenv("TEST_TX_IP");
  const char* rx_ip = std::getenv("TEST_RX_IP");
  const char* eth_dst = std::getenv("TEST_ETH_DST_MAC");
  if (!eth_dst) {
    throw std::runtime_error("TEST_ETH_DST_MAC environment variable not set");
  }
  
  SenderConfig config;
  config.interface_name = "tx_port";
  config.queue_id = 0;
  config.ip_src_addr = tx_ip ? tx_ip : "192.168.10.10";
  config.ip_dst_addr = rx_ip ? rx_ip : "192.168.10.11";
  config.eth_dst_addr = eth_dst;
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

/**
 * @brief Base class for TX/RX roundtrip tests
 * 
 * Provides common setup/teardown logic and test execution for both loopback
 * and physical NIC tests. Subclasses must implement ConfigureInterfaces().
 */
class RoundtripTestBase : public ::testing::Test {
 protected:
  // Common constants
  static constexpr int MASTER_CORE = 3;
  static constexpr int TX_META_BUFFERS = 256;
  static constexpr int RX_META_BUFFERS = 256;
  static constexpr size_t BUF_SIZE = 1064;
  static constexpr size_t NUM_BUFS = 51200;
  static constexpr int TX_BATCH_SIZE = 10240;
  static constexpr int RX_BATCH_SIZE = 10240;
  static constexpr const char* TX_CPU_CORE = "11";
  static constexpr const char* RX_CPU_CORE = "9";
  static constexpr int RX_TIMEOUT_US = 1000;
  
  int cuda_device_ = 0;
  
  /**
   * @brief Initialize CUDA device
   */
  virtual void SetUpCuda() {
    cudaError_t err = cudaSetDevice(cuda_device_);
    ASSERT_EQ(err, cudaSuccess) << "Failed to set CUDA device: " << cudaGetErrorString(err);
  }
  
  /**
   * @brief Configure common Advanced Network settings
   */
  virtual void ConfigureCommonAdvNetSettings(holoscan::advanced_network::NetworkConfig& config) {
    using namespace holoscan::advanced_network;
    config.common_.version = 1;
    config.common_.master_core_ = MASTER_CORE;
    config.common_.manager_type = ManagerType::DPDK;
    config.debug_ = 0;
    config.tx_meta_buffers_ = TX_META_BUFFERS;
    config.rx_meta_buffers_ = RX_META_BUFFERS;
    config.log_level_ = LogLevel::Level::INFO;
  }
  
  /**
   * @brief Configure GPU memory regions for TX and RX
   */
  virtual void ConfigureMemoryRegions(holoscan::advanced_network::NetworkConfig& config) {
    using namespace holoscan::advanced_network;
    // TX memory region
    MemoryRegionConfig tx_mr;
    tx_mr.name_ = "Data_TX_GPU";
    tx_mr.kind_ = MemoryKind::DEVICE;
    tx_mr.affinity_ = 0;
    tx_mr.buf_size_ = BUF_SIZE;
    tx_mr.num_bufs_ = NUM_BUFS;
    tx_mr.owned_ = true;
    config.mrs_["Data_TX_GPU"] = tx_mr;
    
    // RX memory region
    MemoryRegionConfig rx_mr;
    rx_mr.name_ = "Data_RX_GPU";
    rx_mr.kind_ = MemoryKind::DEVICE;
    rx_mr.affinity_ = 0;
    rx_mr.buf_size_ = BUF_SIZE;
    rx_mr.num_bufs_ = NUM_BUFS;
    rx_mr.owned_ = true;
    config.mrs_["Data_RX_GPU"] = rx_mr;
  }
  
  /**
   * @brief Configure network interfaces (must be implemented by subclasses)
   */
  virtual void ConfigureInterfaces(holoscan::advanced_network::NetworkConfig& config) = 0;
  
  /**
   * @brief Common setup: CUDA + Advanced Network initialization
   */
  void SetUp() override {
    SetUpCuda();
    
    using namespace holoscan::advanced_network;
    NetworkConfig adv_net_config;
    
    ConfigureCommonAdvNetSettings(adv_net_config);
    ConfigureMemoryRegions(adv_net_config);
    ConfigureInterfaces(adv_net_config);
    
    auto status = adv_net_init(adv_net_config);
    ASSERT_EQ(status, Status::SUCCESS) 
      << "Failed to initialize Advanced Network manager";
    
    // Note: mgr.run() is called automatically by adv_net_init()
  }
  
  /**
   * @brief Common teardown: shutdown Advanced Network and synchronize CUDA
   */
  void TearDown() override { 
    holoscan::advanced_network::shutdown();
    cudaDeviceSynchronize();
  }
  
  /**
   * @brief Execute roundtrip test with configurable flush phase and logging
   * 
   * @param sender_cfg Sender configuration
   * @param receiver_cfg Receiver configuration
   * @param needs_flush_phase Whether to perform flush loop (required for physical NICs)
   * @param log_prefix Prefix for log messages (e.g., "[Physical NIC] ")
   */
  void RunRoundtripTest(
      SenderConfig sender_cfg,
      ReceiverConfig receiver_cfg,
      bool needs_flush_phase,
      const std::string& log_prefix) {
    // Validate configurations
    ASSERT_NO_THROW(sender_cfg.validate());
    ASSERT_NO_THROW(receiver_cfg.validate());
    
    // Create sender and receiver facades
    std::unique_ptr<IGpuDirectNetworkSender> sender;
    std::unique_ptr<IGpuDirectNetworkReceiver> receiver;
    
    ASSERT_NO_THROW(sender = IGpuDirectNetworkSender::create(sender_cfg));
    ASSERT_NO_THROW(receiver = IGpuDirectNetworkReceiver::create(receiver_cfg));
    
    ASSERT_NE(sender, nullptr);
    ASSERT_NE(receiver, nullptr);
    
    // Allocate GPU buffer for TX
    CudaMemoryGuard tx_buffer(PAYLOAD_SIZE);
    
    // Track received packets
    std::vector<int> received_samples;
    int polls_attempted = 0;
    const int MAX_POLLS = NUM_SAMPLES * 20;
    
    // TX/RX loop: send samples with intervals, poll for received packets
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
      
      // Wait before next sample
      std::this_thread::sleep_for(std::chrono::milliseconds(SAMPLE_INTERVAL_MS));
    }
    
    // Flush phase: ensure all TX bursts are sent to wire (required for physical NICs)
    if (needs_flush_phase) {
      for (int flush_iter = 0; flush_iter < 50; ++flush_iter) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        sender->is_ready();
      }
    }
    
    // Final polling phase: collect remaining packets
    for (int final_poll = 0; final_poll < 20 && polls_attempted < MAX_POLLS; 
         ++final_poll, ++polls_attempted) {
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
    std::cout << log_prefix << "TX Statistics: packets=" << tx_stats.packets_sent 
              << ", bytes=" << tx_stats.bytes_transmitted 
              << ", dropped=" << tx_stats.frames_dropped << std::endl;
    std::cout << log_prefix << "RX Statistics: packets=" << rx_stats.packets_received 
              << ", bytes=" << rx_stats.bytes_received 
              << ", polls=" << rx_stats.polls_attempted 
              << ", empty_polls=" << rx_stats.empty_polls << std::endl;
    std::cout << log_prefix << "Unique received samples: " << received_samples.size() << std::endl;
  }
};

#ifndef TEST_PHYSICAL_ONLY
/**
 * @brief Integration test fixture for TX/RX roundtrip (Loopback Mode)
 */
class TxRxRoundtripTest : public RoundtripTestBase {
 protected:
  void ConfigureInterfaces(holoscan::advanced_network::NetworkConfig& config) override {
    using namespace holoscan::advanced_network;
    
    config.common_.dir = Direction::TX_RX;
    config.common_.loopback_ = LoopbackType::LOOPBACK_TYPE_SW;
    
    InterfaceConfig loopback_if;
    loopback_if.name_ = "loopback_ports";
    loopback_if.address_ = "loopback";
    loopback_if.port_id_ = 0;
    
    // TX queue
    TxQueueConfig tx_q;
    tx_q.common_.id_ = 0;
    tx_q.common_.name_ = "tx_q_0";
    tx_q.common_.batch_size_ = TX_BATCH_SIZE;
    tx_q.common_.cpu_core_ = TX_CPU_CORE;
    tx_q.common_.mrs_.push_back("Data_TX_GPU");
    tx_q.common_.offloads_.push_back("tx_eth_src");
    loopback_if.tx_.queues_.push_back(tx_q);
    
    // RX queue
    RxQueueConfig rx_q;
    rx_q.common_.id_ = 0;
    rx_q.common_.name_ = "rq_q_0";
    rx_q.common_.batch_size_ = RX_BATCH_SIZE;
    rx_q.common_.cpu_core_ = RX_CPU_CORE;
    rx_q.timeout_us_ = RX_TIMEOUT_US;
    rx_q.common_.mrs_.push_back("Data_RX_GPU");
    loopback_if.rx_.queues_.push_back(rx_q);
    
    config.ifs_.push_back(loopback_if);
  }
};

/**
 * @brief Test TX/RX roundtrip with 10 samples, expecting at least 8 received
 */
TEST_F(TxRxRoundtripTest, SendReceive10SamplesExpect8) {
  RunRoundtripTest(
    create_loopback_sender_config(),
    create_loopback_receiver_config(),
    false,  // No flush phase needed for loopback
    ""      // No log prefix
  );
}
#endif // TEST_PHYSICAL_ONLY

#ifndef TEST_LOOPBACK_ONLY
/**
 * @brief Integration test fixture for TX/RX roundtrip using physical NICs
 */
class PhysicalNicRoundtripTest : public RoundtripTestBase {
 protected:
  std::string tx_pcie_;
  std::string rx_pcie_;
  //TODO: mac is not needed here after config changes. Review later.
  std::string rx_mac_;
  
  void SetUp() override {
    // Check for required environment variables
    const char* tx_pcie = std::getenv("TEST_TX_NIC_PCIE");
    const char* rx_pcie = std::getenv("TEST_RX_NIC_PCIE");
    const char* rx_mac = std::getenv("TEST_ETH_DST_MAC");
    
    ASSERT_TRUE(tx_pcie != nullptr) 
      << "TEST_TX_NIC_PCIE environment variable not set. "
      << "Example: export TEST_TX_NIC_PCIE=\"0005:03:00.0\"";
    ASSERT_TRUE(rx_pcie != nullptr) 
      << "TEST_RX_NIC_PCIE environment variable not set. "
      << "Example: export TEST_RX_NIC_PCIE=\"0005:03:00.1\"";
    ASSERT_TRUE(rx_mac != nullptr) 
      << "TEST_ETH_DST_MAC environment variable not set. "
      << "Example: export TEST_RX_MAC=\"3c:6d:66:11:91:56\"";
    
    tx_pcie_ = tx_pcie;
    rx_pcie_ = rx_pcie;
    rx_mac_ = rx_mac;
    
    // Call base class SetUp() after environment variable validation
    RoundtripTestBase::SetUp();
  }
  
  void ConfigureInterfaces(holoscan::advanced_network::NetworkConfig& config) override {
    using namespace holoscan::advanced_network;
    
    config.common_.loopback_ = LoopbackType::DISABLED;
    
    // TX Interface - DPDK will create dummy RX queue automatically
    InterfaceConfig tx_if;
    tx_if.name_ = "tx_port";
    tx_if.address_ = tx_pcie_;
    tx_if.rx_.flow_isolation_ = false;
    
    TxQueueConfig tx_q;
    tx_q.common_.id_ = 0;
    tx_q.common_.name_ = "tx_q_0";
    tx_q.common_.batch_size_ = TX_BATCH_SIZE;
    tx_q.common_.cpu_core_ = TX_CPU_CORE;
    tx_q.common_.mrs_.push_back("Data_TX_GPU");
    tx_q.common_.offloads_.push_back("tx_eth_src");
    tx_if.tx_.queues_.push_back(tx_q);
    
    config.ifs_.push_back(tx_if);
    
    // RX Interface - DPDK will create dummy TX queue automatically
    InterfaceConfig rx_if;
    rx_if.name_ = "rx_port";
    rx_if.address_ = rx_pcie_;
    rx_if.rx_.flow_isolation_ = false;
    
    RxQueueConfig rx_q;
    rx_q.common_.id_ = 0;
    rx_q.common_.name_ = "rx_q_0";
    rx_q.common_.batch_size_ = RX_BATCH_SIZE;
    rx_q.common_.cpu_core_ = RX_CPU_CORE;
    rx_q.timeout_us_ = RX_TIMEOUT_US;
    rx_q.common_.mrs_.push_back("Data_RX_GPU");
    rx_if.rx_.queues_.push_back(rx_q);
    
    config.ifs_.push_back(rx_if);
  }
};

/**
 * @brief Test TX/RX roundtrip with physical NICs: 10 samples, expecting at least 8 received
 */
TEST_F(PhysicalNicRoundtripTest, SendReceive10SamplesExpect8) {
  RunRoundtripTest(
    create_physical_sender_config(),
    create_physical_receiver_config(),
    true,               // Flush phase required for physical NICs
    "[Physical NIC] "   // Log prefix
  );
}
#endif // TEST_LOOPBACK_ONLY

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
