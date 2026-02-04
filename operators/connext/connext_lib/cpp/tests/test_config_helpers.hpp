#pragma once
// Test-only helpers for configuration from environment variables

#include "connext_lib/config/config.hpp"
#include <cstdlib>
#include <string>

namespace connext_lib::test {

inline std::string getenv_or(const char* env, const std::string& def) {
  const char* v = std::getenv(env);
  return v ? std::string(v) : def;
}

/**
 * @brief Create AnoNetworkConfig from environment variables
 * 
 * Used in tests to simulate configuration that would normally come from YAML.
 * 
 * Environment variables:
 * - ANO_INTERFACE: Network interface name (default: "eth0")
 * - ANO_QUEUE_ID: Advanced Network queue ID (default: "0")
 * - ANO_SRC_IP: Source/fast IP address (default: "0.0.0.0")
 * - ANO_SRC_PORT: Source/fast UDP port (default: "4096")
 * - ANO_HEADER_SIZE: Header size in bytes (default: "64")
 * - ANO_MAX_PACKET_SIZE: Max packet size in bytes (default: "9000")
 */
inline connext_lib::AnoNetworkConfig createAnoNetworkConfigFromEnv() {
  connext_lib::AnoNetworkConfig cfg;
  cfg.set_network_interface(getenv_or("ANO_INTERFACE", "eth0"));
  cfg.set_queue_id(static_cast<uint16_t>(std::stoul(getenv_or("ANO_QUEUE_ID", "0"))));
  cfg.set_fast_ip(getenv_or("ANO_SRC_IP", "0.0.0.0"));
  cfg.set_fast_port(std::stoi(getenv_or("ANO_SRC_PORT", "4096")));
  cfg.set_header_size(static_cast<uint16_t>(std::stoul(getenv_or("ANO_HEADER_SIZE", "64"))));
  cfg.set_max_packet_size(static_cast<uint16_t>(std::stoul(getenv_or("ANO_MAX_PACKET_SIZE", "9000"))));
  return cfg;
}

}  // namespace connext_lib::test
