#pragma once
#include <string>
#include <stdexcept>

namespace connext_lib {

/// Minimal destination metadata. This mirrors DestinationInfo used by ANOPayloadWriter.
struct DestinationInfo {
  std::string ip_addr;
  std::string mac_addr;
  uint16_t udp_port = 0;

  // Serialize to a canonical string: {ip}:{port}:{mac}
  std::string toString() const {
    return ip_addr + ":" + std::to_string(udp_port) + ":" + mac_addr;
  }

  // Parse from canonical string: {ip}:{port}:{mac}
  // Throws std::invalid_argument or std::out_of_range on error.
  static DestinationInfo fromString(const std::string& s) {
    DestinationInfo d;
    size_t p1 = s.find(':');
    if (p1 == std::string::npos) {
      throw std::invalid_argument("Invalid destination string: missing first ':'");
    }
    size_t p2 = s.find(':', p1 + 1);
    if (p2 == std::string::npos) {
      throw std::invalid_argument("Invalid destination string: missing second ':'");
    }
    d.ip_addr = s.substr(0, p1);
    int port = std::stoi(s.substr(p1 + 1, p2 - p1 - 1));
    if (port < 0 || port > 65535) {
      throw std::out_of_range("Invalid destination string: port out of range");
    }
    d.udp_port = static_cast<uint16_t>(port);
    d.mac_addr = s.substr(p2 + 1);
    return d;
  }
};

}  // namespace connext_lib