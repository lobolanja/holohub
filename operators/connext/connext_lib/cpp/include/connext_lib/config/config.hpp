#pragma once
#include <string>
namespace connext_lib {
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
class AnoConfig {
 public:
  AnoConfig() = default;
  AnoConfig(std::string channel_name,
            std::string buffer_id,
            std::size_t max_payload_bytes,
            bool enabled);
  [[nodiscard]] const std::string& channel_name() const { return channel_name_; }
  [[nodiscard]] std::size_t max_payload_bytes() const { return max_payload_bytes_; }
  [[nodiscard]] bool enabled() const { return enabled_; }
  [[nodiscard]] const std::string& buffer_id() const {return buffer_id_;}
 private:
  std::string channel_name_{"connext_ano_stream"};
  std::string buffer_id_{"ano_buffer_01"};
  std::size_t max_payload_bytes_{1024};
  bool enabled_{false};
};
}  // namespace connext_lib
