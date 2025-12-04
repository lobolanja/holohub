#include <chrono>
#include <vector>

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
  spec.param(ano_buffer_id_,
             "ano_buffer_id",
             "ANO Buffer ID",
             "Identifier used by ANO shared memory buffers.",
             std::string("ano_buffer_01"));
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
                 ano_buffer_id_.get(), 
                 static_cast<std::size_t>(ano_max_payload_.get()),
                 enable_ano_.get());


}

void ConnextRxOp::start() {
  refresh_configs();

  if (dds_config_.enabled() == false && ano_config_.enabled() == false) {
    throw std::runtime_error("ConnextRxOp requires at least one transport to be enabled (DDS or ANO).");
  } else if (dds_config_.enabled() && ano_config_.enabled()) {
    throw std::runtime_error("ConnextRxOp currently supports only one transport at a time (DDS or ANO).");
  } else if (dds_config_.enabled()) {
    HOLOSCAN_LOG_INFO("ConnextRxOp: DDS transport enabled.");
    dds_reader_ = std::make_unique<connext_lib::ConnextDDSReader>(dds_config_);
  } else if (ano_config_.enabled()) {
    HOLOSCAN_LOG_INFO("ConnextRxOp: ANO transport enabled.");
    constexpr std::chrono::milliseconds kAnoReaderPollInterval{100};
    ano_reader_ = std::make_unique<connext_lib::ConnextANOReader>(
        ano_config_, dds_config_, kAnoReaderPollInterval);
  }

  HOLOSCAN_LOG_INFO(
      "ConnextRxOp starting (dds_enabled={}, ano_enabled={}, channel={})",
      dds_config_.enabled(),
      ano_config_.enabled(),
      ano_config_.channel_name());
}

void ConnextRxOp::stop() {
  dds_reader_.reset();
  ano_reader_.reset();
}

void ConnextRxOp::compute(InputContext& input, OutputContext& output, ExecutionContext& context) {
  (void)input;
  (void)context;

  if (!output_.get()) {
    throw std::runtime_error("ConnextRxOp output IO spec is not set.");
  }

  std::vector<std::uint8_t> received_data;

  if (dds_config_.enabled() && dds_reader_) {
    received_data = dds_reader_->readSamples();
  } else if (ano_config_.enabled() && ano_reader_) {
    received_data = ano_reader_->readSamples();
  }

  if (received_data.empty()) { return; }

  auto payload_storage =
      std::make_shared<std::vector<std::uint8_t>>(std::move(received_data));

  auto entity = nvidia::gxf::Entity::New(context.context());
  if (!entity) {
    throw std::runtime_error("Failed to create GXF entity for received payload.");
  }

  auto payload_tensor = entity.value().add<nvidia::gxf::Tensor>("payload");
  if (!payload_tensor) {
    throw std::runtime_error("Failed to add payload tensor to GXF entity.");
  }

  nvidia::gxf::Shape payload_shape{
      static_cast<int32_t>(payload_storage->size())};

  auto wrap_result = payload_tensor.value()->wrapMemory(
      payload_shape,
      nvidia::gxf::PrimitiveType::kUnsigned8,
      sizeof(std::uint8_t),
      nvidia::gxf::ComputeTrivialStrides(payload_shape, sizeof(std::uint8_t)),
      nvidia::gxf::MemoryStorageType::kSystem,
      payload_storage->data(),
      [payload_storage](void*) mutable {
        payload_storage.reset();
        return nvidia::gxf::Success;
      });

  if (!wrap_result) {
    throw std::runtime_error("Failed to wrap payload buffer in tensor component.");
  }

  auto gxf_entity = gxf::Entity(std::move(entity.value()));
  output.emit(gxf_entity, "output");
}

}  // namespace holoscan::ops
