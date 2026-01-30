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

AnoNetworkConfig::AnoNetworkConfig(bool enabled,
                                   std::string network_interface,
                                   int gpu_device_id,
                                   std::string fast_ip,
                                   std::string fast_mac_address,
                                   int fast_port)
    : enabled_(enabled),
      network_interface_(std::move(network_interface)),
      gpu_device_id_(gpu_device_id),
      fast_ip_(std::move(fast_ip)),
      fast_mac_address_(std::move(fast_mac_address)),
      fast_port_(fast_port) {}

AnoNetworkConfig::AnoNetworkConfig(bool enabled,
                                   std::string network_interface,
                                   int gpu_device_id)
    : enabled_(enabled),
      network_interface_(std::move(network_interface)),
      gpu_device_id_(gpu_device_id) {}

void AnoNetworkConfig::validate() const {
  if (network_interface_.empty()) {
    throw InvalidConfigException("network_interface cannot be empty");
  }
  if (gpu_device_id_ < 0) {
    throw InvalidConfigException("gpu_device_id must be non-negative");
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
