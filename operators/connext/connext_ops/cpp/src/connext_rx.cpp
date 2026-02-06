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
             "Toggle ANO transport with GPU Direct support. "
             "When enabled, outputs GPU tensors (MemoryStorageType::kDevice) for zero-copy "
             "transmission. Requires NVIDIA ConnectX NIC with GPUDirect capability.",
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
  
  // ANO Network Configuration parameters
  spec.param(ano_network_interface_,
             "ano_network_interface",
             "ANO Network Interface",
             "Network interface name for ANO (e.g., 'eth0', 'enp1s0f0')",
             std::string("eth0"));
  spec.param(ano_gpu_device_id_,
             "ano_gpu_device_id",
             "ANO GPU Device ID",
             "CUDA GPU device ID for GPUDirect operations",
             0);
  spec.param(ano_fast_ip_,
             "ano_fast_ip",
             "ANO Fast Path IP",
             "IP address for ANO fast path communication",
             std::string("192.168.10.11"));
  spec.param(ano_fast_mac_address_,
             "ano_fast_mac_address",
             "ANO Fast Path MAC",
             "MAC address for ANO fast path",
             std::string("00:00:00:00:00:00"));
  spec.param(ano_fast_port_,
             "ano_fast_port",
             "ANO Fast Path Port",
             "UDP port for ANO fast path",
             5000);
  spec.param(ano_header_size_,
             "ano_header_size",
             "ANO Header Size",
             "Network header size in bytes (Ethernet+IP+UDP, minimum 42)",
             static_cast<uint16_t>(64));
  spec.param(ano_max_packet_size_,
             "ano_max_packet_size",
             "ANO Max Packet Size",
             "Maximum packet size (MTU constraint, typically 1500 or 9000)",
             static_cast<uint16_t>(9000));
  spec.param(ano_queue_id_,
             "ano_queue_id",
             "ANO Queue ID",
             "Advanced Network RX queue ID",
             static_cast<uint16_t>(0));
}

void ConnextRxOp::refresh_configs() {
  dds_config_.set_enabled(enable_dds_.get());
  dds_config_.set_domain_id(domain_id_.get());
  dds_config_.set_topic_name(topic_name_.get());
  dds_config_.set_topic_type_name(topic_type_name_.get());

  // Build ANO network configuration
  connext_lib::AnoNetworkConfig ano_net_config(
      ano_network_interface_.get(),
      ano_gpu_device_id_.get(),
      ano_fast_ip_.get(),
      ano_fast_mac_address_.get(),
      ano_fast_port_.get());
  
  ano_net_config.set_header_size(ano_header_size_.get());
  ano_net_config.set_max_packet_size(ano_max_packet_size_.get());
  ano_net_config.set_queue_id(ano_queue_id_.get());

  // Build ANO configuration with network config
  ano_config_ = connext_lib::AnoConfig(
      ano_channel_.get(),
      ano_buffer_id_.get(),
      static_cast<std::size_t>(ano_max_payload_.get()),
      enable_ano_.get(),
      ano_net_config);
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

  connext_lib::MemoryBufferView received_buffer{nullptr, 0, false};

  if (dds_config_.enabled() && dds_reader_) {
    received_buffer = dds_reader_->readSamples();
  } else if (ano_config_.enabled() && ano_reader_) {
    received_buffer = ano_reader_->readSamples();
  }

  if (received_buffer.ptr == nullptr || received_buffer.size_bytes == 0) { return; }

  auto entity = nvidia::gxf::Entity::New(context.context());
  if (!entity) {
    throw std::runtime_error("Failed to create GXF entity for received payload.");
  }

  auto payload_tensor = entity.value().add<nvidia::gxf::Tensor>("payload");
  if (!payload_tensor) {
    throw std::runtime_error("Failed to add payload tensor to GXF entity.");
  }

  nvidia::gxf::Shape payload_shape{static_cast<int32_t>(received_buffer.size_bytes)};
  nvidia::gxf::Expected<void> wrap_result;

  // ANO transport: Keep GPU memory (zero-copy GPU Direct)
  // DDS transport: Copy to CPU memory (DDS requires host memory)
  if (received_buffer.is_device && ano_config_.enabled()) {
    // ANO: Wrap GPU pointer directly - enables zero-copy GPU→NIC transmission
    // Buffer freed via deleter lambda when tensor is destroyed
    auto buffer_copy = received_buffer;  // Capture by value
    
    // Create shared_ptr to keep ano_reader_ alive until tensor destruction
    // Uses aliasing constructor with no-op deleter since unique_ptr owns the object
    std::shared_ptr<connext_lib::ConnextANOReader> ano_reader_shared(
        ano_reader_.get(),
        [](connext_lib::ConnextANOReader*) { /* no-op: unique_ptr owns deletion */ });
    
    wrap_result = payload_tensor.value()->wrapMemory(
        payload_shape,
        nvidia::gxf::PrimitiveType::kUnsigned8,
        sizeof(std::uint8_t),
        nvidia::gxf::ComputeTrivialStrides(payload_shape, sizeof(std::uint8_t)),
        nvidia::gxf::MemoryStorageType::kDevice,
        received_buffer.ptr,
        [ano_reader_shared, buffer_copy](void*) mutable {
          if (ano_reader_shared) {
            ano_reader_shared->freeBuffer(buffer_copy);
          }
          return nvidia::gxf::Success;
        });
  } else {
    // DDS: Copy to CPU vector (DDS always returns host memory)
    std::vector<std::uint8_t> received_data(
        static_cast<const std::uint8_t*>(received_buffer.ptr),
        static_cast<const std::uint8_t*>(received_buffer.ptr) + received_buffer.size_bytes);
    
    // Free the buffer after copying
    if (dds_config_.enabled() && dds_reader_) {
      dds_reader_->freeBuffer(received_buffer);
    }

    auto payload_storage =
        std::make_shared<std::vector<std::uint8_t>>(std::move(received_data));

    wrap_result = payload_tensor.value()->wrapMemory(
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
  }

  if (!wrap_result) {
    throw std::runtime_error("Failed to wrap payload buffer in tensor component.");
  }

  auto gxf_entity = gxf::Entity(std::move(entity.value()));
  output.emit(gxf_entity, "output");
}

}  // namespace holoscan::ops
