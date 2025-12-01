#include <chrono>

#include "connext_ops/connext_tx.hpp"

namespace holoscan::ops {

void ConnextTxOp::setup(OperatorSpec& spec) {
  auto& input_spec = spec.input<gxf::Entity>("input");

  spec.param(input_, "input", "Input", "Input channel carrying payloads.", &input_spec);
  spec.param(enable_dds_,
             "enable_dds",
             "Enable DDS",
             "Toggle DDS transport usage.",
             true);
  spec.param(domain_id_, "domain_id", "DDS Domain ID", "DDS domain identifier.", 0);
  spec.param(topic_name_,
             "topic_name",
             "DDS Topic Name",
             "DDS topic used to publish payload metadata.",
             std::string("system_setup"));
  spec.param(topic_type_name_,
             "topic_type_name",
             "DDS Topic Type",
             "DDS topic type used for payload metadata.",
             std::string(""));
  spec.param(enable_ano_,
             "enable_ano",
             "Enable ANO transport",
             "Toggle ANO transport fallback.",
             false);
  spec.param(ano_channel_,
             "ano_channel",
             "ANO Channel",
             "Logical ANO channel name.",
             std::string("connext_ano_stream"));
  spec.param(ano_max_payload_,
             "ano_max_payload",
             "Max ANO Payload (bytes)",
             "Maximum payload size expected over ANO.",
             static_cast<uint64_t>(1024));
  spec.param(destination_reference_,
             "destination_reference",
             "Destination Reference",
             "Optional destination hint that writers use.",
             std::string(""));
}

void ConnextTxOp::refresh_configs() {
  dds_config_.set_enabled(enable_dds_.get());
  dds_config_.set_domain_id(domain_id_.get());
  dds_config_.set_topic_name(topic_name_.get());
  dds_config_.set_topic_type_name(topic_type_name_.get());

  ano_config_ =
      connext_lib::AnoConfig(ano_channel_.get(),
                              "",
                             static_cast<std::size_t>(ano_max_payload_.get()),
                             enable_ano_.get());
}

void ConnextTxOp::start() {
  Operator::start();
  refresh_configs();

  if (!dds_config_.enabled() && !ano_config_.enabled()) {
    throw std::runtime_error(
        "ConnextTxOp requires at least one transport to be enabled (DDS or ANO).");
  } else if (dds_config_.enabled() && ano_config_.enabled()) {
    throw std::runtime_error(
        "ConnextTxOp currently supports only one transport at a time (DDS or ANO).");
  }

  if (dds_config_.enabled()) {
    dds_writer_ = std::make_unique<connext_lib::ConnextDDSWriter>(
        dds_config_, static_cast<int>(ano_max_payload_.get()));
  } else if (ano_config_.enabled()) {
    constexpr std::chrono::milliseconds kAnoWriterPollInterval{100};
    ano_writer_ = std::make_unique<connext_lib::ConnextANOWriter>(
        ano_config_, dds_config_, kAnoWriterPollInterval);
  }

  HOLOSCAN_LOG_INFO(
      "ConnextTxOp starting (dds_enabled={}, ano_enabled={}, channel={}, destination={})",
      dds_config_.enabled(),
      ano_config_.enabled(),
      ano_config_.channel_name(),
      destination_reference_.get());
}

void ConnextTxOp::stop() {
  dds_writer_.reset();
  ano_writer_.reset();
  Operator::stop();
}

void ConnextTxOp::compute(InputContext& input, OutputContext& output, ExecutionContext& context) {
  (void)output;
  (void)context;

  auto entity_expected = input.receive<gxf::Entity>("input");
  if (!entity_expected) { return; }

  auto tensor = entity_expected.value().get<Tensor>("payload");
  if (!tensor) { return; }

  auto* data = static_cast<std::uint8_t*>(tensor->data());
  if (!data) { return; }

  const auto payload_size = static_cast<std::size_t>(tensor->nbytes());
  connext_lib::PayloadBufferView buffer{data, payload_size};

  if (dds_config_.enabled() && dds_writer_) {
    dds_writer_->broadcast(buffer);
  } else if (ano_config_.enabled() && ano_writer_) {
    ano_writer_->broadcast(buffer);
  }
}

}  // namespace holoscan::ops
