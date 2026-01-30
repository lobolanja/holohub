#pragma once
// payload_transport_ano.hpp
// ANO-backed implementation of PayloadWriterInterface (GPU-Direct support)

#include "connext_lib/transport/payload_transport.hpp"
#include "connext_lib/config/config.hpp"
#include "dds/dds.hpp"
#include <dds/domain/DomainParticipant.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>

#include "connext_lib/transport/sender_info.hpp"

#include "connext_lib/transport/sender_factory.hpp"

// Forward-declare holoscan ANO interfaces to avoid heavy includes in this header
namespace holoscan {
namespace ops {
class IGpuDirectNetworkSender;
class IGpuDirectNetworkReceiver;
}  // namespace ops
}  // namespace holoscan

namespace connext_lib {

using SenderDestination = DestinationInfo; // alias for clarity

/// Lightweight buffer descriptor used by the writer.
struct BufferViewLocal {
  void* ptr = nullptr;   ///< Pointer to buffer.  this must be a CUDA device pointer.
  std::size_t size = 0;  ///< Number of bytes.
};

class ISenderFactory; // forward

class ANOPayloadWriter : public PayloadWriterInterface {
 public:
  ~ANOPayloadWriter() override;

  // Constructor: domain participant handle, writer options, and ANO config.
  ANOPayloadWriter(dds::domain::DomainParticipant dp,
                   const PayloadWriterOptions& opts,
                   const AnoConfig& ano_config,
                   std::shared_ptr<ISenderFactory> sender_factory = nullptr);

  // PayloadWriterInterface (legacy writeTo for destination key)
  void setBuffer(const PayloadBufferView& buffer) override;
  bool writeTo(const std::string& destination_reference) override;

  // New: register destination info (called by resource manager upon discovery)
  // The key is the canonical destination reference string (for example: "<ip>:<port>:<mac>").
  void registerDestination(const std::string& destination_reference, const SenderDestination& dest);
  void unregisterDestination(const std::string& destination_reference);
  // Buffer staging: callers use the generic `setBuffer(const PayloadBufferView&)`.
  // If `PayloadBufferView::is_device == true` then `device_ptr` will be used
  // as a CUDA device pointer and no host copy will be made. Otherwise the
  // payload is copied into an internal host staging buffer.

 private:
  // Helper: get or create sender for given destination_reference; returns nullptr on error.
  std::shared_ptr<holoscan::ops::IGpuDirectNetworkSender> getOrCreateSenderFor(const std::string& destination_reference);

  AnoConfig ano_config_;
  std::size_t max_payload_bytes_{0};
  dds::domain::DomainParticipant dp_;

  // staging
  void* staged_gpu_ptr_{nullptr};
  std::size_t staged_gpu_capacity_{0};
  std::size_t staged_size_{0};

  std::mutex writer_mutex_;

  // Discovery state: maps the canonical destination_reference -> DestinationInfo
  std::unordered_map<std::string, SenderDestination> dest_map_;

  // Cache of per-destination senders
  std::unordered_map<std::string, std::shared_ptr<holoscan::ops::IGpuDirectNetworkSender>> sender_cache_;

  std::shared_ptr<ISenderFactory> sender_factory_;
};

// Factory helper to create a PayloadWriterInterface backed by ANO.
std::unique_ptr<PayloadWriterInterface> MakeANOPayloadWriter(
    dds::domain::DomainParticipant dp,
    const PayloadWriterOptions& opts,
    const AnoConfig& ano_config,
    std::shared_ptr<ISenderFactory> sender_factory = nullptr);

// ANO-backed PayloadReader: returns GPU pointer and size from IGpuDirectNetworkReceiver
class ANOPayloadReader : public PayloadReaderInterface {
 public:
  ANOPayloadReader(dds::domain::DomainParticipant dp,
           const PayloadReaderOptions& opts,
           const AnoConfig& ano_config,
           std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> receiver = nullptr);
  ~ANOPayloadReader() override;

  // PayloadReaderInterface
  bool readNext(void*& data_ptr, std::size_t& size,
        std::chrono::milliseconds timeout) override;
  void freeData(void* data_ptr) override;

 private:
  AnoConfig ano_config_;
  dds::domain::DomainParticipant dp_;
  std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> receiver_;
};

// Factory helper to create a PayloadReaderInterface backed by ANO.
std::unique_ptr<PayloadReaderInterface> MakeANOPayloadReader(
  dds::domain::DomainParticipant dp,
  const PayloadReaderOptions& opts,
  const AnoConfig& ano_config);
std::unique_ptr<ANOPayloadReader> MakeANOPayloadReader(dds::domain::DomainParticipant& participant,
                                                      const PayloadReaderOptions& opts,
                                                      const AnoConfig& ano_config,
                                                      std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> receiver = nullptr);

}  // namespace connext_lib
