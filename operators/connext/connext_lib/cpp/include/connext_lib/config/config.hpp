#pragma once
#include <string>
#include <stdexcept>
#include <connext_ano_lib/sender_config.h> // For SendMode enum

/**
 * @file config.hpp
 * @brief Configuration types for Connext library DDS and ANO transports
 */

namespace connext_lib {

/**
 * @class InvalidConfigException
 * @brief Exception thrown when configuration validation fails
 */
class InvalidConfigException : public std::runtime_error {
 public:
  explicit InvalidConfigException(const std::string& message)
      : std::runtime_error("Invalid configuration: " + message) {}
};

/**
 * @class DdsConfig
 * @brief Configuration for DDS (Data Distribution Service) transport
 *
 * DDS provides standards-based publish-subscribe communication with built-in
 * discovery, quality of service, and data type safety. This configuration
 * specifies the DDS domain and topic settings.
 *
 * @note DDS is used for both control plane (discovery/announcements) and
 *       can also be used as the primary data transport.
 */
class DdsConfig {
 public:
  /**
   * @brief Default constructor with standard defaults
   */
  DdsConfig() = default;
  
  /**
   * @brief Construct DDS configuration with all parameters
   * @param enabled Whether DDS transport is enabled
   * @param domain_id DDS domain ID (0-232, isolates DDS networks)
   * @param topic_name Name of the DDS topic for data exchange
   * @param topic_type_name Type name for the DDS topic (typically "BytesTopicType")
   */
  DdsConfig(bool enabled,
            int domain_id,
            std::string topic_name,
            std::string topic_type_name);
  
  /** @brief Check if DDS transport is enabled */
  [[nodiscard]] bool enabled() const { return enabled_; }
  /** @brief Enable or disable DDS transport */
  void set_enabled(bool value) { enabled_ = value; }
  
  /** @brief Get DDS domain ID */
  [[nodiscard]] int domain_id() const { return domain_id_; }
  /** @brief Set DDS domain ID (0-232) */
  void set_domain_id(int value) { domain_id_ = value; }
  
  /** @brief Get DDS topic name */
  [[nodiscard]] const std::string& topic_name() const { return topic_name_; }
  /** @brief Set DDS topic name */
  void set_topic_name(std::string value) { topic_name_ = std::move(value); }
  
  /** @brief Get DDS topic type name */
  [[nodiscard]] const std::string& topic_type_name() const { return topic_type_name_; }
  /** @brief Set DDS topic type name */
  void set_topic_type_name(std::string value) {
    topic_type_name_ = std::move(value);
  }
  
 private:
  bool enabled_{false};                           ///< DDS enabled flag
  int domain_id_{0};                              ///< DDS domain ID (default: 0)
  std::string topic_name_{"system_setup"};       ///< Topic name (default: "system_setup")
  std::string topic_type_name_{};                 ///< Topic type name
};

/**
 * @class AnoNetworkConfig
 * @brief Network configuration for ANO (Advanced Network Objects) transport
 *
 * ANO provides high-performance, zero-copy data transfer over RDMA-capable
 * networks. This configuration specifies the network interface, GPU device,
 * and network parameters for ANO communication.
 *
 * @note ANO requires specialized hardware (ConnectX NICs) and NVIDIA GPUs
 *       with GPUDirect support for optimal performance.
 */
class AnoNetworkConfig {
 public:
  /**
   * @brief Default constructor with standard defaults
   */
  AnoNetworkConfig() = default;
  
  /**
   * @brief Construct ANO network configuration with full parameters
   * @param network_interface Network interface name (e.g., "eth0", "enp1s0f0")
   * @param gpu_device_id CUDA GPU device ID for GPUDirect operations
   * @param fast_ip IP address for ANO fast path communication
   * @param fast_mac_address MAC address for ANO fast path
   * @param fast_port UDP port for ANO fast path
   */
  AnoNetworkConfig(std::string network_interface,
                   int gpu_device_id,
                   std::string fast_ip,
                   std::string fast_mac_address,
                   int fast_port);
  
  /**
   * @brief Construct ANO network configuration with minimal parameters
   * @param network_interface Network interface name
   * @param gpu_device_id CUDA GPU device ID
   * @note Uses default values for IP, MAC, and port
   */
  AnoNetworkConfig(std::string network_interface,
                   int gpu_device_id);
  
  /** @brief Get network interface name */
  [[nodiscard]] const std::string& network_interface() const {
    return network_interface_;
  }
  /** @brief Set network interface name */
  void set_network_interface(std::string value) {
    network_interface_ = std::move(value);
  }
  
  /** @brief Get GPU device ID */
  [[nodiscard]] int gpu_device_id() const { return gpu_device_id_; }
  /** @brief Set GPU device ID for GPUDirect operations */
  void set_gpu_device_id(int value) { gpu_device_id_ = value; }
  
  /** @brief Get ANO fast path IP address */
  [[nodiscard]] const std::string& fast_ip() const { return fast_ip_; }
  /** @brief Set ANO fast path IP address */
  void set_fast_ip(std::string value) { fast_ip_ = std::move(value); }
  
  /** @brief Get ANO fast path MAC address */
  [[nodiscard]] const std::string& fast_mac_address() const { return fast_mac_address_; }
  /** @brief Set ANO fast path MAC address */
  void set_fast_mac_address(std::string value) { fast_mac_address_ = std::move(value); }
  
  /** @brief Get ANO fast path UDP port */
  [[nodiscard]] int fast_port() const { return fast_port_; }
  /** @brief Set ANO fast path UDP port */
  void set_fast_port(int value) { fast_port_ = value; }
  
  /** @brief Get network header size in bytes (Ethernet+IP+UDP) */
  [[nodiscard]] uint16_t header_size() const { return header_size_; }
  /** @brief Set network header size (minimum 42 bytes) */
  void set_header_size(uint16_t value) { header_size_ = value; }
  
  /** @brief Get maximum packet size (MTU constraint) */
  [[nodiscard]] uint16_t max_packet_size() const { return max_packet_size_; }
  /** @brief Set maximum packet size (typically 1500 or 9000 for jumbo frames) */
  void set_max_packet_size(uint16_t value) { max_packet_size_ = value; }
  
  /** @brief Get Advanced Network TX/RX queue ID */
  [[nodiscard]] uint16_t queue_id() const { return queue_id_; }
  /** @brief Set Advanced Network TX/RX queue ID */
  void set_queue_id(uint16_t value) { queue_id_ = value; }
  
  /** @brief Get send mode (IMMEDIATE or BATCH) */
  [[nodiscard]] holoscan::ops::SendMode send_mode() const { return send_mode_; }
  /** @brief Set send mode for transmission behavior */
  void set_send_mode(holoscan::ops::SendMode value) { send_mode_ = value; }
  
  /**
   * @brief Validate configuration parameters
   * @throws InvalidConfigException if configuration is invalid
   */
  void validate() const;
  
 private:
  std::string network_interface_{"eth0"};           ///< Network interface name
  int gpu_device_id_{0};                             ///< CUDA GPU device ID
  std::string fast_ip_{"127.0.0.1"};                ///< ANO fast path IP
  std::string fast_mac_address_{"00:00:00:00:00:00"}; ///< ANO fast path MAC
  int fast_port_{5000};                              ///< ANO fast path UDP port
  
  uint16_t header_size_{64};                         ///< Ethernet+IP+UDP header size (min 42)
  uint16_t max_packet_size_{9000};                   ///< MTU constraint (1500 standard, 9000 jumbo)
  uint16_t queue_id_{0};                             ///< Advanced Network TX/RX queue ID
  holoscan::ops::SendMode send_mode_{holoscan::ops::SendMode::IMMEDIATE};  ///< Send mode (default: IMMEDIATE)
};

/**
 * @class AnoConfig
 * @brief Configuration for ANO (Advanced Network Objects) transport
 *
 * ANO enables high-performance, zero-copy payload transfer over RDMA networks
 * with GPUDirect support. This configuration specifies the logical channel,
 * buffer settings, and underlying network configuration.
 *
 * ANO is typically used in conjunction with DDS, where DDS handles discovery
 * and control plane, while ANO handles high-throughput data plane transfers.
 *
 * @note Requires compatible hardware: ConnectX NICs and NVIDIA GPUs with
 *       GPUDirect RDMA support.
 */
class AnoConfig {
 public:
  /**
   * @brief Default constructor with standard defaults
   */
  AnoConfig() = default;
  
  /**
   * @brief Construct ANO configuration without network config
   * @param channel_name Logical channel name for ANO communication
   * @param buffer_id Unique identifier for the ANO buffer
   * @param max_payload_bytes Maximum size of a single payload in bytes
   * @param enabled Whether ANO transport is enabled
   * @note Uses default AnoNetworkConfig (disabled)
   */
  AnoConfig(std::string channel_name,
            std::string buffer_id,
            std::size_t max_payload_bytes,
            bool enabled);
  
  /**
   * @brief Construct ANO configuration with full network config
   * @param channel_name Logical channel name for ANO communication
   * @param buffer_id Unique identifier for the ANO buffer
   * @param max_payload_bytes Maximum size of a single payload in bytes
   * @param enabled Whether ANO transport is enabled
   * @param ano_network_config Network-level ANO configuration
   */
  AnoConfig(std::string channel_name,
            std::string buffer_id,
            std::size_t max_payload_bytes,
            bool enabled,
            AnoNetworkConfig ano_network_config);
  
  /** @brief Get logical channel name */
  [[nodiscard]] const std::string& channel_name() const { return channel_name_; }
  
  /** @brief Get maximum payload size in bytes */
  [[nodiscard]] std::size_t max_payload_bytes() const { return max_payload_bytes_; }
  
  /** @brief Check if ANO transport is enabled */
  [[nodiscard]] bool enabled() const { return enabled_; }
  
  /** @brief Get ANO buffer identifier */
  [[nodiscard]] const std::string& buffer_id() const {return buffer_id_;}
  
  /** @brief Get ANO network configuration */
  [[nodiscard]] const AnoNetworkConfig& ano_network_config() const {
    return ano_network_config_;
  }
  
 private:
  std::string channel_name_{"connext_ano_stream"};  ///< Logical channel name
  std::string buffer_id_{"ano_buffer_01"};          ///< Buffer identifier
  std::size_t max_payload_bytes_{1024};              ///< Maximum payload size
  bool enabled_{false};                              ///< ANO enabled flag
  AnoNetworkConfig ano_network_config_{"eth0", 0}; ///< Network configuration
  
};
}  // namespace connext_lib
