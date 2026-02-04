#include "connext_lib/transport/sender_factory.hpp"
#include "connext_lib/config/config.hpp"
#include "connext_ano_lib/gpu_direct_network_sender.h"
#include <holoscan/logger/logger.hpp>

namespace connext_lib {

SenderFactory::SenderFactory(const AnoNetworkConfig& config)
    : config_(&config) {}

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
    // Fill destination-specific fields (from discovery)
    cfg.ip_dst_addr = dest.ip_addr;
    cfg.eth_dst_addr = dest.mac_addr;
    cfg.udp_dst_port = dest.udp_port;

    // Fill fields from config
    cfg.interface_name = config_->network_interface();
    cfg.queue_id = config_->queue_id();
    cfg.header_size = config_->header_size();
    cfg.max_packet_size = config_->max_packet_size();
    cfg.send_mode = config_->send_mode();
    
    // Source address/port come from the local receiver's advertised address
    cfg.ip_src_addr = config_->fast_ip();
    cfg.udp_src_port = static_cast<uint16_t>(config_->fast_port());

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
