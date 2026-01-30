#pragma once
#include <string>
#include <stdexcept>
namespace connext_lib {

class InvalidConfigException : public std::runtime_error {
 public:
  explicit InvalidConfigException(const std::string& message)
      : std::runtime_error("Invalid configuration: " + message) {}
};

class DdsConfig {
 public:
  DdsConfig() = default;
  DdsConfig(bool enabled,
            int domain_id,
            std::string topic_name,
            std::string topic_type_name);
  [[nodiscard]] bool enabled() const { return enabled_; }
  void set_enabled(bool value) { enabled_ = value; }
  [[nodiscard]] int domain_id() const { return domain_id_; }
  void set_domain_id(int value) { domain_id_ = value; }
  [[nodiscard]] const std::string& topic_name() const { return topic_name_; }
  void set_topic_name(std::string value) { topic_name_ = std::move(value); }
  [[nodiscard]] const std::string& topic_type_name() const { return topic_type_name_; }
  void set_topic_type_name(std::string value) {
    topic_type_name_ = std::move(value);
  }
 private:
  bool enabled_{false};
  int domain_id_{0};
  std::string topic_name_{"system_setup"};
  std::string topic_type_name_{};
};

class AnoNetworkConfig {
 public:
  AnoNetworkConfig() = default;
  AnoNetworkConfig(bool enabled,
                   std::string network_interface,
                   int gpu_device_id,
                   std::string fast_ip,
                   std::string fast_mac_address,
                   int fast_port);
  AnoNetworkConfig(bool enabled,
                   std::string network_interface,
                   int gpu_device_id);
  
  [[nodiscard]] bool enabled() const { return enabled_; }
  void set_enabled(bool value) { enabled_ = value; }
  [[nodiscard]] const std::string& network_interface() const {
    return network_interface_;
  }
  void set_network_interface(std::string value) {
    network_interface_ = std::move(value);
  }
  [[nodiscard]] int gpu_device_id() const { return gpu_device_id_; }
  void set_gpu_device_id(int value) { gpu_device_id_ = value; }
  [[nodiscard]] const std::string& fast_ip() const { return fast_ip_; }
  void set_fast_ip(std::string value) { fast_ip_ = std::move(value); }
  [[nodiscard]] const std::string& fast_mac_address() const { return fast_mac_address_; }
  void set_fast_mac_address(std::string value) { fast_mac_address_ = std::move(value); }
  [[nodiscard]] int fast_port() const { return fast_port_; }
  void set_fast_port(int value) { fast_port_ = value; }
  
  void validate() const;
  
 private:
  bool enabled_{false};
  std::string network_interface_{"eth0"};
  int gpu_device_id_{0};
  std::string fast_ip_{"127.0.0.1"};
  std::string fast_mac_address_{"00:00:00:00:00:00"};
  int fast_port_{5000};
};

class AnoConfig {
 public:
  AnoConfig() = default;
  AnoConfig(std::string channel_name,
            std::string buffer_id,
            std::size_t max_payload_bytes,
            bool enabled);
  AnoConfig(std::string channel_name,
            std::string buffer_id,
            std::size_t max_payload_bytes,
            bool enabled,
            AnoNetworkConfig ano_network_config);
  [[nodiscard]] const std::string& channel_name() const { return channel_name_; }
  [[nodiscard]] std::size_t max_payload_bytes() const { return max_payload_bytes_; }
  [[nodiscard]] bool enabled() const { return enabled_; }
  [[nodiscard]] const std::string& buffer_id() const {return buffer_id_;}
  [[nodiscard]] const AnoNetworkConfig& ano_network_config() const {
    return ano_network_config_;
  }
 private:
  std::string channel_name_{"connext_ano_stream"};
  std::string buffer_id_{"ano_buffer_01"};
  std::size_t max_payload_bytes_{1024};
  bool enabled_{false};
  AnoNetworkConfig ano_network_config_{false, "eth0", 0};
  
};
}  // namespace connext_lib
