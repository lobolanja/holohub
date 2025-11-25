#include "connext_lib/config/config.hpp"
#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/comm/connext_tx.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
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
    auto payload_writer = std::make_unique<DdsPayloadWriter>(dp, topic_name, ano_config.max_payload_bytes());
    tx_ = std::make_unique<ConnextTx>(std::move(sender_manager), std::move(payload_writer), poll_interval_ms_);
}

std::size_t ConnextANOWriter::broadcast(const PayloadBufferView& buffer) {
    if (!tx_) return false;
    tx_->setBuffer(buffer);
    return tx_->broadcast();
}

ConnextDDSWriter::ConnextDDSWriter(const DdsConfig& dds_config, int max_payload_bytes){
  int domain_id = dds_config.domain_id();
  std::string topic_name = dds_config.topic_name();
  dds::domain::DomainParticipant dp(domain_id);
  payload_writer_ = std::make_unique<DdsPayloadWriter>(dp,topic_name,max_payload_bytes);

}

ConnextDDSWriter::~ConnextDDSWriter() = default;

std::size_t ConnextDDSWriter::broadcast(const PayloadBufferView& buffer) {
    payload_writer_->setBuffer(buffer);
    return payload_writer_->writeTo("*") ? 1 : 0;
}

} // namespace connext_lib
