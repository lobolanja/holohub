#include "connext_lib/config/config.hpp"
#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/comm/connext_tx.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/transport/payload_transport_ano.hpp"
#include <memory>
#include <stdexcept>
#include <vector>
#include <chrono>

namespace connext_lib {

ConnextANOWriter::ConnextANOWriter(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms)
    : poll_interval_ms_(poll_interval_ms) {
    int domain_id = dds_config.domain_id();
    std::string topic_name = "GPU/" + dds_config.topic_name();
    dds::domain::DomainParticipant dp(domain_id);
    auto sender_manager = std::make_unique<DdsSenderResourcesManager>(dp, ano_config.channel_name());
    auto payload_writer = MakeANOPayloadWriter(dp, PayloadWriterOptions{ano_config.channel_name(), ano_config.max_payload_bytes()}, ano_config);
    tx_ = std::make_unique<ConnextTx>(std::move(sender_manager), std::move(payload_writer), poll_interval_ms_);
}

std::size_t ConnextANOWriter::broadcast(const MemoryBufferView& buffer) {
    if (!tx_) return 0;
    
    // Convert MemoryBufferView to PayloadBufferView
    // ANO typically expects GPU memory, but can handle CPU if needed
    PayloadBufferView payload_view;
    if (buffer.is_device) {
        payload_view.device_ptr = buffer.ptr;
        payload_view.data = nullptr;
    } else {
        // CPU memory - still use device_ptr field for ANO compatibility
        payload_view.device_ptr = buffer.ptr;
        payload_view.data = nullptr;
    }
    payload_view.size_bytes = buffer.size_bytes;
    
    tx_->setBuffer(payload_view);
    return tx_->broadcast();
}

ConnextDDSWriter::ConnextDDSWriter(const DdsConfig& dds_config, int max_payload_bytes){
  int domain_id = dds_config.domain_id();
  std::string topic_name = dds_config.topic_name();
  dds::domain::DomainParticipant dp(domain_id);
  payload_writer_ = std::make_unique<DdsPayloadWriter>(dp,topic_name,max_payload_bytes);

}

ConnextDDSWriter::~ConnextDDSWriter() = default;

std::size_t ConnextDDSWriter::broadcast(const MemoryBufferView& buffer) {
    // Convert MemoryBufferView to PayloadBufferView
    PayloadBufferView payload_view;
    if (buffer.is_device) {
        // GPU memory - use device_ptr
        payload_view.device_ptr = buffer.ptr;
        payload_view.data = nullptr;
    } else {
        // CPU memory - use data pointer
        payload_view.data = static_cast<const std::uint8_t*>(buffer.ptr);
        payload_view.device_ptr = nullptr;
    }
    payload_view.size_bytes = buffer.size_bytes;
    
    payload_writer_->setBuffer(payload_view);
    return payload_writer_->writeTo("*") ? 1 : 0;
}

} // namespace connext_lib
