#include "connext_ops/connext_rx.hpp"

namespace holoscan::ops {

void ConnextRxOp::setup(OperatorSpec& spec) {
  auto& output_spec = spec.output<gxf::Entity>("output");

  spec.param(output_, "output", "Output", "Output channel used for payloads.", &output_spec);
  spec.param(enable_dds_,
             "enable_dds",
             "Enable DDS",
             "Toggle DDS transport usage.",
             true);
  spec.param(domain_id_, "domain_id", "DDS Domain ID", "DDS domain identifier.", 0);
  spec.param(topic_name_,
             "topic_name",
             "DDS Topic Name",
             "DDS topic used to receive payload metadata.",
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
}

void ConnextRxOp::refresh_configs() {
  dds_config_.set_enabled(enable_dds_.get());
  dds_config_.set_domain_id(domain_id_.get());
  dds_config_.set_topic_name(topic_name_.get());
  dds_config_.set_topic_type_name(topic_type_name_.get());

  ano_config_ =
      connext_lib::AnoConfig(ano_channel_.get(),
                             static_cast<std::size_t>(ano_max_payload_.get()),
                             enable_ano_.get());
}

void ConnextRxOp::start() {
  Operator::start();
  refresh_configs();
  HOLOSCAN_LOG_INFO(
      "ConnextRxOp starting (dds_enabled={}, ano_enabled={}, channel={})",
      dds_config_.enabled(),
      ano_config_.enabled(),
      ano_config_.channel_name());
}

void ConnextRxOp::stop() {
  payload_reader_.reset();
  payload_transport_.reset();
  Operator::stop();
}

void ConnextRxOp::compute(InputContext& input, OutputContext& output, ExecutionContext& context) {
  (void)input;
  (void)output;
  (void)context;
  // No-op for now – payload handling will be implemented in follow-up changes.
}

}  // namespace holoscan::ops
