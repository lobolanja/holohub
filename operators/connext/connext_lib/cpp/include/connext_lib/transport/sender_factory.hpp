#pragma once
// sender_factory.hpp
// Abstraction for creating ANO senders to allow test injection.

#include <memory>
#include <string>
#include "connext_lib/transport/sender_info.hpp"

namespace holoscan { namespace ops { struct IGpuDirectNetworkSender; struct SenderConfig; }}

namespace connext_lib {

class AnoNetworkConfig;  // forward declaration

class ISenderFactory {
 public:
  virtual ~ISenderFactory() = default;
  // Create a sender for the given destination reference (legacy string-based key).
  virtual std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> create_sender(const std::string& destination_reference) = 0;
  // Modern overload: create sender directly from structured DestinationInfo.
  virtual std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> create_sender(const DestinationInfo& dest) = 0;

  // NOTE: sender creation uses a string key that represents the destination
  // (for example: "<ip>:<port>:<mac>"). Resource discovery code should register
  // the DestinationInfo with the writer and the writer will build the canonical
  // key and call this string-based create_sender.
};

// Production factory will be implemented in .cpp and call into connext_ano_lib.
class SenderFactory : public ISenderFactory {
 public:
  // Constructor accepting config
  explicit SenderFactory(const AnoNetworkConfig& config);
  
  std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> create_sender(const std::string& destination_reference) override;
  std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> create_sender(const DestinationInfo& dest) override;
  
 private:
  const AnoNetworkConfig* config_;
};

}  // namespace connext_lib
