/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "connext_lib/config/config.hpp"
#include "advanced_network/common.h"
#include <cuda_runtime.h>
#include <chrono>
#include <string>
#include <vector>
#include <cstdlib>

namespace connext_lib {
namespace test {

/**
 * @brief Helper class for managing ANO initialization in tests.
 * 
 * Handles detection of physical NIC vs loopback mode, DPDK initialization,
 * and provides environment-based configuration for TX/RX interfaces.
 */
class AnoInitializer {
public:
  /**
   * @brief Initialize ANO with physical NICs or loopback mode.
   * 
   * Physical mode requires environment variables:
   *   - TEST_TX_NIC_PCIE: TX NIC PCIe address
   *   - TEST_RX_NIC_PCIE: RX NIC PCIe address
   *   - TEST_ETH_DST_MAC: Destination MAC address (optional)
   *   - TEST_TX_IP: TX IP address (optional, default 192.168.10.10)
   *   - TEST_RX_IP: RX IP address (optional, default 192.168.10.11)
   * 
   * If PCIe addresses are not set, falls back to loopback mode.
   * 
   * @return true if initialization succeeded, false otherwise
   */
  static bool Initialize() {
    if (initialized_) return true;
    
    // Get IP addresses from environment with defaults
    const char* tx_ip = std::getenv("TEST_TX_IP");
    const char* rx_ip = std::getenv("TEST_RX_IP");
    tx_ip_addr_ = tx_ip ? tx_ip : "192.168.10.10";
    rx_ip_addr_ = rx_ip ? rx_ip : "192.168.10.11";
    
    const char* eth_dst = std::getenv("TEST_ETH_DST_MAC");
    eth_dst_mac_ = eth_dst ? eth_dst : "3c:6d:66:11:91:55";  // Port 0's MAC for loopback
    
    // Detect physical NIC mode vs loopback
    const char* tx_pcie = std::getenv("TEST_TX_NIC_PCIE");
    const char* rx_pcie = std::getenv("TEST_RX_NIC_PCIE");
    
    if (tx_pcie && rx_pcie) {
      // Physical NIC mode
      is_physical_mode_ = true;
      tx_pcie_addr_ = tx_pcie;
      rx_pcie_addr_ = rx_pcie;
      return InitializePhysical();
    } else {
      // Loopback mode
      is_physical_mode_ = false;
      return InitializeLoopback();
    }
  }
  
  /**
   * @brief Shutdown ANO and cleanup resources.
   */
  static void Shutdown() {
    if (initialized_) {
      holoscan::advanced_network::shutdown();
      cudaDeviceSynchronize();
      initialized_ = false;
    }
  }
  
  static bool IsPhysicalMode() { return is_physical_mode_; }
  static std::string GetTxIp() { return tx_ip_addr_; }
  static std::string GetRxIp() { return rx_ip_addr_; }
  static std::string GetEthDstMac() { return eth_dst_mac_; }
  
private:
  static bool InitializePhysical() {
    try {
      using namespace holoscan::advanced_network;
      
      // Initialize CUDA
      cudaError_t err = cudaSetDevice(0);
      if (err != cudaSuccess) {
        return false;
      }
      
      NetworkConfig config;
      
      // Common settings - Physical NICs
      config.common_.version = 1;
      config.common_.master_core_ = 3;
      config.common_.manager_type = ManagerType::DPDK;
      config.common_.loopback_ = LoopbackType::DISABLED;
      config.debug_ = 0;
      config.tx_meta_buffers_ = 256;
      config.rx_meta_buffers_ = 2048;  // Increased for high-frequency tests
      config.log_level_ = LogLevel::Level::INFO;
      
      // Memory regions
      MemoryRegionConfig tx_mr;
      tx_mr.name_ = "Data_TX_GPU";
      tx_mr.kind_ = MemoryKind::DEVICE;
      tx_mr.affinity_ = 0;
      tx_mr.buf_size_ = 1064;
      tx_mr.num_bufs_ = 51200;
      tx_mr.owned_ = true;
      config.mrs_["Data_TX_GPU"] = tx_mr;
      
      MemoryRegionConfig rx_mr;
      rx_mr.name_ = "Data_RX_GPU";
      rx_mr.kind_ = MemoryKind::DEVICE;
      rx_mr.affinity_ = 0;
      rx_mr.buf_size_ = 1064;
      rx_mr.num_bufs_ = 51200;
      rx_mr.owned_ = true;
      config.mrs_["Data_RX_GPU"] = rx_mr;
      
      // TX Interface
      InterfaceConfig tx_if;
      tx_if.name_ = "tx_port";
      tx_if.address_ = tx_pcie_addr_;
      tx_if.rx_.flow_isolation_ = false;
      
      TxQueueConfig tx_q;
      tx_q.common_.id_ = 0;
      tx_q.common_.name_ = "tx_q_0";
      tx_q.common_.batch_size_ = 10240;
      tx_q.common_.cpu_core_ = "11";
      tx_q.common_.mrs_.push_back("Data_TX_GPU");
      tx_q.common_.offloads_.push_back("tx_eth_src");
      tx_if.tx_.queues_.push_back(tx_q);
      
      config.ifs_.push_back(tx_if);
      
      // RX Interface
      InterfaceConfig rx_if;
      rx_if.name_ = "rx_port";
      rx_if.address_ = rx_pcie_addr_;
      rx_if.rx_.flow_isolation_ = true;  // Enable hardware flow steering
      
      // RX Queue 0 - for UDP port 4096 (test[0] and test[1])
      RxQueueConfig rx_q0;
      rx_q0.common_.id_ = 0;
      rx_q0.common_.name_ = "rx_q_0";
      rx_q0.common_.batch_size_ = 10240;
      rx_q0.common_.cpu_core_ = "9";
      rx_q0.timeout_us_ = 1000;
      rx_q0.common_.mrs_.push_back("Data_RX_GPU");
      rx_if.rx_.queues_.push_back(rx_q0);
      
      // RX Queue 1 - for UDP port 4097 (test[2] - receiver 2)
      RxQueueConfig rx_q1;
      rx_q1.common_.id_ = 1;
      rx_q1.common_.name_ = "rx_q_1";
      rx_q1.common_.batch_size_ = 10240;
      rx_q1.common_.cpu_core_ = "9";
      rx_q1.timeout_us_ = 1000;
      rx_q1.common_.mrs_.push_back("Data_RX_GPU");
      rx_if.rx_.queues_.push_back(rx_q1);
      
      // RX Queue 2 - for UDP port 4098 (test[2] - receiver 3)
      RxQueueConfig rx_q2;
      rx_q2.common_.id_ = 2;
      rx_q2.common_.name_ = "rx_q_2";
      rx_q2.common_.batch_size_ = 10240;
      rx_q2.common_.cpu_core_ = "9";
      rx_q2.timeout_us_ = 1000;
      rx_q2.common_.mrs_.push_back("Data_RX_GPU");
      rx_if.rx_.queues_.push_back(rx_q2);
      
      // Configure flow steering to filter by UDP source and destination ports
      // Each flow directs packets to a specific queue based on UDP destination port
      
      // Flow 1: Match UDP 4096->4096 → Queue 0 (test[0] and test[1])
      FlowConfig flow1;
      flow1.name_ = "ano_test_flow_4096";
      flow1.id_ = 0;
      flow1.action_.type_ = FlowType::QUEUE;
      flow1.action_.id_ = 0;  // → Queue 0
      flow1.match_.udp_src_ = 4096;  // Source port (from sender) - required > 0 for UDP filtering
      flow1.match_.udp_dst_ = 4096;  // Destination port (to receiver)
      flow1.match_.ipv4_len_ = 0;    // 0 means ANY (don't filter by IP length)
      rx_if.rx_.flows_.push_back(flow1);
      
      // Flow 2: Match UDP 4096->4097 → Queue 1 (test[2] - receiver 2)
      FlowConfig flow2;
      flow2.name_ = "ano_test_flow_4097";
      flow2.id_ = 1;
      flow2.action_.type_ = FlowType::QUEUE;
      flow2.action_.id_ = 1;  // → Queue 1
      flow2.match_.udp_src_ = 4096;
      flow2.match_.udp_dst_ = 4097;
      flow2.match_.ipv4_len_ = 0;
      rx_if.rx_.flows_.push_back(flow2);
      
      // Flow 3: Match UDP 4096->4098 → Queue 2 (test[2] - receiver 3)
      FlowConfig flow3;
      flow3.name_ = "ano_test_flow_4098";
      flow3.id_ = 2;
      flow3.action_.type_ = FlowType::QUEUE;
      flow3.action_.id_ = 2;  // → Queue 2
      flow3.match_.udp_src_ = 4096;
      flow3.match_.udp_dst_ = 4098;
      flow3.match_.ipv4_len_ = 0;
      rx_if.rx_.flows_.push_back(flow3);
      
      config.ifs_.push_back(rx_if);
      
      auto status = adv_net_init(config);
      if (status != Status::SUCCESS) {
        return false;
      }
      
      initialized_ = true;
      return true;
    } catch (...) {
      return false;
    }
  }
  
  static bool InitializeLoopback() {
    try {
      using namespace holoscan::advanced_network;
      
      // Initialize CUDA
      cudaError_t err = cudaSetDevice(0);
      if (err != cudaSuccess) {
        return false;
      }
      
      NetworkConfig config;
      
      // Common settings - use loopback for testing
      config.common_.version = 1;
      config.common_.master_core_ = 3;
      config.common_.manager_type = ManagerType::DPDK;
      config.common_.dir = Direction::TX_RX;
      config.common_.loopback_ = LoopbackType::LOOPBACK_TYPE_SW;
      config.debug_ = 0;
      config.tx_meta_buffers_ = 256;
      config.rx_meta_buffers_ = 256;
      config.log_level_ = LogLevel::Level::INFO;
      
      // Memory regions
      MemoryRegionConfig tx_mr;
      tx_mr.name_ = "Data_TX_GPU";
      tx_mr.kind_ = MemoryKind::DEVICE;
      tx_mr.affinity_ = 0;
      tx_mr.buf_size_ = 1064;
      tx_mr.num_bufs_ = 51200;
      tx_mr.owned_ = true;
      config.mrs_["Data_TX_GPU"] = tx_mr;
      
      MemoryRegionConfig rx_mr;
      rx_mr.name_ = "Data_RX_GPU";
      rx_mr.kind_ = MemoryKind::DEVICE;
      rx_mr.affinity_ = 0;
      rx_mr.buf_size_ = 1064;
      rx_mr.num_bufs_ = 51200;
      rx_mr.owned_ = true;
      config.mrs_["Data_RX_GPU"] = rx_mr;
      
      // Loopback interface configuration
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
      rx_q.common_.name_ = "rx_q_0";
      rx_q.common_.batch_size_ = 10240;
      rx_q.common_.cpu_core_ = "9";
      rx_q.timeout_us_ = 1000;
      rx_q.common_.mrs_.push_back("Data_RX_GPU");
      loopback_if.rx_.queues_.push_back(rx_q);
      
      config.ifs_.push_back(loopback_if);
      
      auto status = adv_net_init(config);
      if (status != Status::SUCCESS) {
        return false;
      }
      
      initialized_ = true;
      return true;
    } catch (...) {
      return false;
    }
  }
  
private:
  static bool initialized_;
  static bool is_physical_mode_;
  static std::string tx_ip_addr_;
  static std::string rx_ip_addr_;
  static std::string eth_dst_mac_;
  static std::string tx_pcie_addr_;
  static std::string rx_pcie_addr_;
};

// Static member definitions
bool AnoInitializer::initialized_ = false;
bool AnoInitializer::is_physical_mode_ = false;
std::string AnoInitializer::tx_ip_addr_;
std::string AnoInitializer::rx_ip_addr_;
std::string AnoInitializer::eth_dst_mac_;
std::string AnoInitializer::tx_pcie_addr_;
std::string AnoInitializer::rx_pcie_addr_;

/**
 * @brief Helper class for creating standard ANO test configurations.
 * 
 * Provides factory methods to create common ANO/DDS configurations with
 * standard defaults, eliminating code duplication across tests.
 */
class AnoTestConfigHelper {
public:
  // Standard configuration constants
  static constexpr uint16_t kDefaultHeaderSize = 64;
  static constexpr uint16_t kDefaultMaxPacketSize = 1064;
  static constexpr int kDefaultPollIntervalMs = 50;
  static constexpr uint16_t kDefaultStartPort = 4096;
  static constexpr uint16_t kDefaultQueueId = 0;
  static constexpr int kDefaultGpuDeviceId = 0;
  
  /**
   * @brief Create a standard reader network configuration.
   * 
   * Creates an AnoNetworkConfig for receiving data with standard settings:
   * - Header size: 64 bytes
   * - Max packet size: 1064 bytes
   * - Network interface: "rx_port" (physical) or "loopback_ports" (loopback)
   * - IP/MAC from AnoInitializer environment
   * 
   * @param port UDP destination port (default 4096)
   * @param queue_id RX queue ID (default 0)
   * @return Configured AnoNetworkConfig for reader
   */
  static AnoNetworkConfig CreateDefaultReaderNetworkConfig(
      uint16_t port = kDefaultStartPort,
      uint16_t queue_id = kDefaultQueueId) {
    AnoNetworkConfig config;
    config.set_network_interface(AnoInitializer::IsPhysicalMode() ? "rx_port" : "loopback_ports");
    config.set_queue_id(queue_id);
    config.set_header_size(kDefaultHeaderSize);
    config.set_max_packet_size(kDefaultMaxPacketSize);
    config.set_fast_ip(AnoInitializer::GetRxIp());
    config.set_fast_mac_address(AnoInitializer::GetEthDstMac());
    config.set_fast_port(port);
    return config;
  }
  
  /**
   * @brief Create a standard writer network configuration.
   * 
   * Creates an AnoNetworkConfig for sending data with standard settings:
   * - Header size: 64 bytes
   * - Max packet size: 1064 bytes
   * - Network interface: "tx_port" (physical) or "loopback_ports" (loopback)
   * - IP from AnoInitializer environment
   * - GPU device ID: 0
   * 
   * @param port UDP source port (default 4096)
   * @param queue_id TX queue ID (default 0)
   * @return Configured AnoNetworkConfig for writer
   */
  static AnoNetworkConfig CreateDefaultWriterNetworkConfig(
      uint16_t port = kDefaultStartPort,
      uint16_t queue_id = kDefaultQueueId) {
    AnoNetworkConfig config;
    config.set_network_interface(AnoInitializer::IsPhysicalMode() ? "tx_port" : "loopback_ports");
    config.set_queue_id(queue_id);
    config.set_header_size(kDefaultHeaderSize);
    config.set_max_packet_size(kDefaultMaxPacketSize);
    config.set_fast_ip(AnoInitializer::GetTxIp());
    config.set_fast_port(port);
    config.set_gpu_device_id(kDefaultGpuDeviceId);
    return config;
  }
  
  /**
   * @brief Create a standard ANO configuration.
   * 
   * Wraps network configuration with channel, buffer, and ANO enable flag.
   * 
   * @param channel DDS channel name
   * @param buffer_id Unique buffer identifier
   * @param buffer_size Buffer size in bytes
   * @param net_config Network configuration (from CreateDefaultReaderNetworkConfig or CreateDefaultWriterNetworkConfig)
   * @param ano_enabled Enable ANO mode (default true)
   * @return Configured AnoConfig
   */
  static AnoConfig CreateStandardAnoConfig(
      const std::string& channel,
      const std::string& buffer_id,
      size_t buffer_size,
      const AnoNetworkConfig& net_config,
      bool ano_enabled = true) {
    return AnoConfig(channel, buffer_id, buffer_size, ano_enabled, net_config);
  }
  
  /**
   * @brief Create a standard DDS configuration.
   * 
   * Creates a DdsConfig with standard topic type and ANO enabled.
   * 
   * @param domain DDS domain ID
   * @param channel DDS channel/topic name
   * @param topic_type DDS topic type (default "BytesTopicType")
   * @param ano_enabled Enable ANO mode (default true)
   * @return Configured DdsConfig
   */
  static DdsConfig CreateStandardDdsConfig(
      int domain,
      const std::string& channel,
      const std::string& topic_type = "BytesTopicType",
      bool ano_enabled = true) {
    return DdsConfig(ano_enabled, domain, channel, topic_type);
  }
  
  /**
   * @brief Get standard poll interval for tests.
   * 
   * @return 50ms poll interval
   */
  static std::chrono::milliseconds GetStandardPollInterval() {
    return std::chrono::milliseconds(kDefaultPollIntervalMs);
  }
  
  /**
   * @brief Create multiple reader network configurations for multi-receiver tests.
   * 
   * Creates a vector of AnoNetworkConfig objects, each with incrementing
   * UDP ports and queue IDs. Useful for tests with multiple independent receivers.
   * 
   * Example: CreateMultipleReaderConfigs(3) creates configs for:
   *   - Reader 0: port 4096, queue 0
   *   - Reader 1: port 4097, queue 1
   *   - Reader 2: port 4098, queue 2
   * 
   * @param num_readers Number of reader configurations to create
   * @param start_port Starting UDP port (default 4096)
   * @param start_queue Starting queue ID (default 0)
   * @return Vector of AnoNetworkConfig objects
   */
  static std::vector<AnoNetworkConfig> CreateMultipleReaderConfigs(
      size_t num_readers,
      uint16_t start_port = kDefaultStartPort,
      uint16_t start_queue = kDefaultQueueId) {
    std::vector<AnoNetworkConfig> configs;
    configs.reserve(num_readers);
    for (size_t i = 0; i < num_readers; ++i) {
      configs.push_back(CreateDefaultReaderNetworkConfig(
          start_port + static_cast<uint16_t>(i),
          start_queue + static_cast<uint16_t>(i)));
    }
    return configs;
  }
};

}  // namespace test
}  // namespace connext_lib
