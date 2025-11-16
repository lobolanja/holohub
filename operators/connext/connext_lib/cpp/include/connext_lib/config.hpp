#pragma once

#include <string>

namespace connext_lib {

/** Configuration shared across the native C++ controllers so higher layers can
 * swap transports without reworking constructors.
 */
class DdsConfig {
 public:
  DdsConfig() = default;
  DdsConfig(bool enabled,
            int domain_id,
            std::string topic_name,
            std::string topic_type_name);

  bool enabled() const { return enabled_; }
  void set_enabled(bool value) { enabled_ = value; }

  int domain_id() const { return domain_id_; }
  void set_domain_id(int value) { domain_id_ = value; }

  const std::string& topic_name() const { return topic_name_; }
  void set_topic_name(std::string value) { topic_name_ = std::move(value); }

  const std::string& topic_type_name() const { return topic_type_name_; }
  void set_topic_type_name(std::string value) {
    topic_type_name_ = std::move(value);
  }

 private:
  bool enabled_{false};
  int domain_id_{0};
  std::string topic_name_{"system_setup"};
  std::string topic_type_name_{};
};

/** Placeholder ANO transport configuration.
 *
 * Future ANO transports (RDMA/DPDK) will inject their details here; for now
 * this simply identifies a logical channel riding on DDS.
 */
class AnoConfig {
 public:
  AnoConfig() = default;
  AnoConfig(std::string channel_name,
            std::size_t max_payload_bytes,
            bool enabled);

  const std::string& channel_name() const { return channel_name_; }
  std::size_t max_payload_bytes() const { return max_payload_bytes_; }
  bool enabled() const { return enabled_; }

 private:
  std::string channel_name_{"connext_ano_stream"};
  std::size_t max_payload_bytes_{1024};
  bool enabled_{false};
};

}  // namespace connext_lib
