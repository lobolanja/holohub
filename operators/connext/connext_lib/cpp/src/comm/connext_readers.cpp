#include "connext_lib/comm/connext_readers.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/transport/payload_transport_ano.hpp"
#include "connext_lib/comm/connext_rx.hpp"
#include <memory>
#include <vector>
#include <chrono>
#include <cstring>

namespace connext_lib {

ConnextDDSReader::ConnextDDSReader(const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms)
:poll_interval_ms_(poll_interval_ms) {
  int domain_id = dds_config.domain_id();
  std::string topic_name = dds_config.topic_name();

  dds::domain::DomainParticipant dp(domain_id);

  payload_reader_ = std::make_unique<DdsPayloadReader>(dp, topic_name,"*");
}

MemoryBufferView ConnextDDSReader::readSamples() const {
    void* data_ptr = nullptr;
    std::size_t size = 0;
    if (payload_reader_->readNext(data_ptr, size, poll_interval_ms_)) {
        if (data_ptr && size > 0) {
            // Return pointer without copying - caller must call freeBuffer()
            // DDS readers return CPU memory
            return {data_ptr, size, false};
        }
    }
    return {nullptr, 0, false};
}

void ConnextDDSReader::freeBuffer(const MemoryBufferView& buffer) const {
    if (buffer.ptr != nullptr) {
        payload_reader_->freeData(buffer.ptr);
    }
}

ConnextANOReader::ConnextANOReader(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms)
    : poll_interval_ms_(poll_interval_ms) {

    int domain_id = dds_config.domain_id();
    
    // Create DDS-based resource manager for discovery
    dds::domain::DomainParticipant dp(domain_id);
    std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager =
        std::make_unique<DdsReceiverResourcesManager>(dp, ano_config);
    
    // Create ANO payload reader from AnoConfig (buffer id / network config inside)
    std::unique_ptr<PayloadReaderInterface> payload_reader =
        MakeANOPayloadReader(ano_config);
    
    rx_ = std::make_unique<ConnextRx>(std::move(receiver_manager), std::move(payload_reader));
}

MemoryBufferView ConnextANOReader::readSamples() const {
    if (rx_) {
        return rx_->receive(poll_interval_ms_);
    } else {
        throw std::runtime_error("ConnextANOReader: receiver not initialized");
    }
    return {nullptr, 0, false};
}

void ConnextANOReader::freeBuffer(const MemoryBufferView& buffer) const {
    if (rx_) {
        rx_->freeBuffer(buffer);
    }
}

} // namespace connext_lib
