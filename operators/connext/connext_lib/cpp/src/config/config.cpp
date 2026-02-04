#include "connext_lib/config/config.hpp"

namespace connext_lib {

DdsConfig::DdsConfig(bool enabled,
                     int domain_id,
                     std::string topic_name,
                     std::string topic_type_name)
    : enabled_(enabled),
      domain_id_(domain_id),
      topic_name_(std::move(topic_name)),
      topic_type_name_(std::move(topic_type_name)) {}

AnoNetworkConfig::AnoNetworkConfig(std::string network_interface,
                                   int gpu_device_id,
                                   std::string fast_ip,
                                   std::string fast_mac_address,
                                   int fast_port)
    : network_interface_(std::move(network_interface)),
      gpu_device_id_(gpu_device_id),
      fast_ip_(std::move(fast_ip)),
      fast_mac_address_(std::move(fast_mac_address)),
      fast_port_(fast_port) {}

AnoNetworkConfig::AnoNetworkConfig(std::string network_interface,
                                   int gpu_device_id)
    : network_interface_(std::move(network_interface)),
      gpu_device_id_(gpu_device_id) {}

void AnoNetworkConfig::validate() const {
  if (network_interface_.empty()) {
    throw InvalidConfigException("network_interface cannot be empty");
  }
  if (gpu_device_id_ < 0) {
    throw InvalidConfigException("gpu_device_id must be non-negative");
  }
  
  constexpr uint16_t MIN_HEADER_SIZE = 42;  // Eth(14) + IP(20) + UDP(8)
  if (header_size_ < MIN_HEADER_SIZE) {
    throw InvalidConfigException(
        "header_size must be at least " + std::to_string(MIN_HEADER_SIZE) + 
        " bytes (Ethernet + IPv4 + UDP), got " + std::to_string(header_size_));
  }
  
  if (max_packet_size_ <= header_size_) {
    throw InvalidConfigException(
        "max_packet_size (" + std::to_string(max_packet_size_) + 
        ") must be greater than header_size (" + std::to_string(header_size_) + ")");
  }
}

AnoConfig::AnoConfig(std::string channel_name,
                     std::string buffer_id,
                     std::size_t max_payload_bytes,
                     bool enabled)
    : channel_name_(std::move(channel_name)),
      buffer_id_(std::move(buffer_id)),
      max_payload_bytes_(max_payload_bytes),
      enabled_(enabled) {}

AnoConfig::AnoConfig(std::string channel_name,
                     std::string buffer_id,
                     std::size_t max_payload_bytes,
                     bool enabled,
                     AnoNetworkConfig ano_network_config)
    : channel_name_(std::move(channel_name)),
      buffer_id_(std::move(buffer_id)),
      max_payload_bytes_(max_payload_bytes),
      enabled_(enabled),
      ano_network_config_(std::move(ano_network_config)) {}

}  // namespace connext_lib
