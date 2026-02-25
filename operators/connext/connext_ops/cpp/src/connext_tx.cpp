#include <chrono>

#include "connext_ops/connext_tx.hpp"

namespace holoscan::ops {

void ConnextTxOp::setup(OperatorSpec& spec) {
  auto& input_spec = spec.input<gxf::Entity>("input");

  spec.param(input_, "input", "Input", "Input channel carrying payloads.", &input_spec);
  spec.param(enable_dds_,
             "enable_dds",
             "Enable DDS",
             "Toggle DDS transport. Accepts both CPU and GPU tensors; GPU tensors are "
             "automatically copied to CPU (incurs performance cost). For best performance, "
             "provide CPU tensors (MemoryStorageType::kSystem) when using DDS.",
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
             "Toggle ANO transport with GPU Direct RDMA. REQUIRES GPU tensors "
             "(MemoryStorageType::kDevice) for zero-copy transmission. Will throw error "
             "if CPU tensor is received. Requires NVIDIA ConnectX NIC with GPUDirect RDMA.",
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
  spec.param(destination_reference_,
             "destination_reference",
             "Destination Reference",
             "Optional destination hint that writers use.",
             std::string(""));
  
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
             std::string("192.168.10.10"));
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
             "Advanced Network TX queue ID",
             static_cast<uint16_t>(0));
}

void ConnextTxOp::refresh_configs() {
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

void ConnextTxOp::start() {
  Operator::start();
  refresh_configs();

  if (!dds_config_.enabled() && !ano_config_.enabled()) {
    throw std::runtime_error(
        "ConnextTxOp requires at least one transport to be enabled (DDS or ANO).");
  }

  if (dds_config_.enabled()) {
    dds_writer_ = std::make_unique<connext_lib::ConnextDDSWriter>(
        dds_config_, static_cast<int>(ano_max_payload_.get()));
  }

  if (ano_config_.enabled()) {
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
  
  // Check if tensor is on GPU device using DLPack device type
  const bool is_gpu_tensor = (tensor->device().device_type == kDLCUDA);

  if (ano_config_.enabled() && ano_writer_) {
    if (!is_gpu_tensor) {
      throw std::runtime_error(
          "ConnextTxOp: ANO transport requires GPU memory (MemoryStorageType::kDevice), "
          "but received CPU tensor. Ensure upstream operator outputs GPU tensors for ANO mode.");
    }
    connext_lib::MemoryBufferView buffer{data, payload_size, true};
    ano_writer_->broadcast(buffer);
  }

  if (dds_config_.enabled() && dds_writer_) {
    if (is_gpu_tensor) {
      std::vector<std::uint8_t> cpu_data(payload_size);
      cudaError_t err = cudaMemcpy(
          cpu_data.data(), data, payload_size, cudaMemcpyDeviceToHost);
      if (err != cudaSuccess) {
        throw std::runtime_error(
            std::string("ConnextTxOp: Failed to copy GPU tensor to CPU for DDS: ") +
            cudaGetErrorString(err));
      }
      connext_lib::MemoryBufferView buffer{cpu_data.data(), payload_size, false};
      dds_writer_->broadcast(buffer);
    } else {
      connext_lib::MemoryBufferView buffer{data, payload_size, false};
      dds_writer_->broadcast(buffer);
    }
  }
}

}  // namespace holoscan::ops
