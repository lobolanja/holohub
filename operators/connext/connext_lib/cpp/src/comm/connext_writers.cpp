#include "connext_lib/config/config.hpp"
#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/comm/connext_tx.hpp"
#include "connext_lib/resource/resource_managers_idl.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/transport/payload_transport_ano.hpp"
#include "dds/dds.hpp"
#include <memory>
#include <stdexcept>
#include <vector>
#include <chrono>

namespace connext_lib {

ConnextANOWriter::ConnextANOWriter(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms)
    : poll_interval_ms_(poll_interval_ms) {
    int domain_id = dds_config.domain_id();
    dds::domain::DomainParticipant dp(domain_id);
    auto sender_manager = std::make_unique<DdsIdlSenderResourcesManager>(dp, ano_config.channel_name());
    auto payload_writer = MakeANOPayloadWriter(ano_config);
    tx_ = std::make_unique<ConnextTx>(std::move(sender_manager), std::move(payload_writer), poll_interval_ms_);
}

std::size_t ConnextANOWriter::broadcast(const MemoryBufferView& buffer) {
    if (!tx_) return 0;
    
    // Pass MemoryBufferView directly to the payload writer
    tx_->setBuffer(buffer);
    return tx_->broadcast();
}

int ConnextANOWriter::flush(int timeout_ms) {
    if (!tx_) return 0;
    return tx_->flush(timeout_ms);
}

ConnextDDSWriter::ConnextDDSWriter(const DdsConfig& dds_config, int max_payload_bytes){
  int domain_id = dds_config.domain_id();
  std::string topic_name = dds_config.topic_name();
  dds::domain::DomainParticipant dp(domain_id);
  payload_writer_ = std::make_unique<DdsPayloadWriter>(dp,topic_name,max_payload_bytes);

}

ConnextDDSWriter::~ConnextDDSWriter() = default;

std::size_t ConnextDDSWriter::broadcast(const MemoryBufferView& buffer) {
    // Pass MemoryBufferView directly to the payload writer
    payload_writer_->setBuffer(buffer);
    return payload_writer_->writeTo("*") ? 1 : 0;
}

dds::pub::DataWriter<dds::core::BytesTopicType>* ConnextDDSWriter::get_dds_writer() {
    auto* dds_writer = dynamic_cast<DdsPayloadWriter*>(payload_writer_.get());
    return dds_writer ? &dds_writer->get_dds_writer() : nullptr;
}

} // namespace connext_lib
