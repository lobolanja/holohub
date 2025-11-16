#include "connext_lib/config.hpp"

namespace connext_lib {

DdsConfig::DdsConfig(bool enabled,
                     int domain_id,
                     std::string topic_name,
                     std::string topic_type_name)
    : enabled_(enabled),
      domain_id_(domain_id),
      topic_name_(std::move(topic_name)),
      topic_type_name_(std::move(topic_type_name)) {}

AnoConfig::AnoConfig(std::string channel_name,
                     std::size_t max_payload_bytes,
                     bool enabled)
    : channel_name_(std::move(channel_name)),
      max_payload_bytes_(max_payload_bytes),
      enabled_(enabled) {}

}  // namespace connext_lib
