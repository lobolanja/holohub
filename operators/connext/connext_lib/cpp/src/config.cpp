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

/// ANO should only be considered active when both the config enables it and
/// some component explicitly turns it on.
void TransportState::ActivateAno() {
  if (config_.enabled()) {
    ano_active_ = true;
  }
}

void TransportState::DeactivateAno() { ano_active_ = false; }

bool TransportState::ShouldUseAno() const {
  return ano_active_ && config_.enabled();
}

}  // namespace connext_lib
