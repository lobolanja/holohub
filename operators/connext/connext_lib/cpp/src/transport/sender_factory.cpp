#include "connext_lib/transport/sender_factory.hpp"
#include "connext_ano_lib/gpu_direct_network_sender.h"
#include <holoscan/logger/logger.hpp>
#include <cstdlib>
#include <sstream>

static std::string getenv_or(const char* env, const std::string& def) {
  const char* v = std::getenv(env);
  return v ? std::string(v) : def;
}

namespace connext_lib {

std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> SenderFactory::create_sender(const std::string& destination_reference) {
  
  try {
    DestinationInfo dest;
    dest.fromString(destination_reference);
    return create_sender(dest);
  } catch (const std::exception& e) {
    HOLOSCAN_LOG_DEBUG("SenderFactory: failed to parse destination_reference '{}': {}", destination_reference, e.what());
    return nullptr;
  }
}

std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> SenderFactory::create_sender(const DestinationInfo& dest) {
  try {
    holoscan::ops::SenderConfig cfg;
    // Fill destination-specific fields
    cfg.ip_dst_addr = dest.ip_addr;
    cfg.eth_dst_addr = dest.mac_addr;
    cfg.udp_dst_port = dest.udp_port;

    // Fill other fields from environment or sensible defaults
    //TODO: get them from the yaml file once that is supported
    cfg.interface_name = getenv_or("ANO_INTERFACE", "eth0");
    cfg.queue_id = static_cast<uint16_t>(std::stoul(getenv_or("ANO_QUEUE_ID", "0")));
    cfg.ip_src_addr = getenv_or("ANO_SRC_IP", "0.0.0.0");
    cfg.udp_src_port = static_cast<uint16_t>(std::stoul(getenv_or("ANO_SRC_PORT", "4096")));
    cfg.header_size = static_cast<uint16_t>(std::stoul(getenv_or("ANO_HEADER_SIZE", "64")));
    cfg.max_packet_size = static_cast<uint16_t>(std::stoul(getenv_or("ANO_MAX_PACKET_SIZE", "9000")));

    cfg.validate();
    auto up = holoscan::ops::IGpuDirectNetworkSender::create(cfg);
    HOLOSCAN_LOG_DEBUG("SenderFactory: created sender for {}:{} ({})", dest.ip_addr, dest.udp_port, dest.mac_addr);
    return up;
  } catch (const std::exception& e) {
    HOLOSCAN_LOG_DEBUG("SenderFactory: failed to create sender: {}", e.what());
    return nullptr;
  }
}

}  // namespace connext_lib
