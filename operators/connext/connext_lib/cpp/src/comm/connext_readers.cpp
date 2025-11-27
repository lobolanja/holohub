#include "connext_lib/comm/connext_readers.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/comm/connext_rx.hpp"
#include <memory>
#include <vector>
#include <chrono>

namespace connext_lib {

ConnextDDSReader::ConnextDDSReader(const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms)
:poll_interval_ms_(poll_interval_ms) {
  int domain_id = dds_config.domain_id();
  std::string topic_name = dds_config.topic_name();

  dds::domain::DomainParticipant dp(domain_id);

  payload_reader_ = std::make_unique<DdsPayloadReader>(dp, topic_name,"*");
}

std::vector<std::uint8_t> ConnextDDSReader::readSamples() const {
    std::vector<std::uint8_t> data;
    payload_reader_->readNext(data, poll_interval_ms_); // timeout configurable
    return data;
}

ConnextANOReader::ConnextANOReader(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms)
    : poll_interval_ms_(poll_interval_ms) {

    // For now, use DDS-based as placeholder (replace with ANO when available)
    int domain_id = dds_config.domain_id();
    std::string topic_name = "GPU/"+dds_config.topic_name();
    dds::domain::DomainParticipant dp(domain_id);
    //TODO: for the resource manager, the dds topic used is the one described un the channel_name. We may want to change this in the future to us the topic_name described above.
    std::unique_ptr<ReceiverResourcesManagerInterface> receiver_manager =
      std::make_unique<DdsReceiverResourcesManager>(
          dp, ano_config.buffer_id(), ano_config.channel_name());
    // TODO: instantiate correct ANO payload reader
    std::unique_ptr<PayloadReaderInterface> payload_reader =
        std::make_unique<DdsPayloadReader>(dp, topic_name, ano_config.buffer_id());
    rx_ = std::make_unique<ConnextRx>(std::move(receiver_manager), std::move(payload_reader));
}

std::vector<std::uint8_t> ConnextANOReader::readSamples() const {
    std::vector<std::uint8_t> data;
    if (rx_) {
        rx_->receive(data, poll_interval_ms_);
    }
    return data;
}

} // namespace connext_lib
