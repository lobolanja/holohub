/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/tensor_network_rx_op.h"
#include <gpu_direct_exceptions.h>

namespace holoscan::ops {

TensorNetworkRxOp::~TensorNetworkRxOp() {
  if (receiver_) {
    const auto stats = receiver_->get_stats();
    HOLOSCAN_LOG_INFO("TensorNetworkRxOp shutting down. Received {} packets ({} bytes, {} polls, {} empty)",
                      stats.packets_received, stats.bytes_received, 
                      stats.polls_attempted, stats.empty_polls);
  }
}

void TensorNetworkRxOp::setup(OperatorSpec& spec) {
  spec.output<std::shared_ptr<holoscan::Tensor>>("tensor_out");
  
  spec.param<std::string>(interface_name_, "interface_name", "Interface Name",
             "Name of NIC interface from advanced_network config");
  spec.param<uint16_t>(max_packet_size_, "max_packet_size", "Max Packet Size",
             "Maximum packet size expected from sender", 9000);
  spec.param<uint16_t>(header_size_, "header_size", "Header Size",
             "Header size on each packet from L4 and below", 42);
  spec.param<int>(gpu_device_, "gpu_device", "GPU Device",
             "GPU device ID", 0);
  spec.param<uint64_t>(max_count_, "max_count", "Maximum Count",
             "Maximum number of packets to receive (0=unlimited)",
             static_cast<uint64_t>(0));
}

void TensorNetworkRxOp::initialize() {
  HOLOSCAN_LOG_INFO("TensorNetworkRxOp::initialize()");
  holoscan::Operator::initialize();

  try {
    // Build configuration
    ReceiverConfig config{
      .interface_name = interface_name_.get(),
      .header_size = header_size_.get(),
      .max_packet_size = max_packet_size_.get(),
      .gpu_device = gpu_device_.get()
    };
    
    // Create receiver facade
    receiver_ = IGpuDirectNetworkReceiver::create(config);
    
    HOLOSCAN_LOG_INFO("TensorNetworkRxOp initialized: interface={}, GPU-only mode, max_payload={}",
                      interface_name_.get(), receiver_->max_payload_size());
    
  } catch (const InvalidConfigException& e) {
    throw std::runtime_error(
      fmt::format("Invalid TensorNetworkRxOp configuration: {}", e.what()));
  } catch (const NetworkInitException& e) {
    throw std::runtime_error(
      fmt::format("Failed to initialize network receiver: {}", e.what()));
  } catch (const CudaInitException& e) {
    throw std::runtime_error(
      fmt::format("Failed to initialize CUDA for receiver: {}", e.what()));
  }
}

void TensorNetworkRxOp::compute(InputContext& op_input, OutputContext& op_output,
                                ExecutionContext& context) {
  // Receive packet (facade handles all DPDK/CUDA complexity)
  auto received = receiver_->receive();
  
  if (!received.has_value()) {
    // No data available this cycle
    return;
  }
  
  auto& data = received.value();
  void* gpu_payload = data.gpu_payload;
  size_t payload_size = data.payload_bytes;
  
  packets_received_++;
  
  // Check max_count limit
  if (max_count_.get() > 0 && packets_received_ >= max_count_.get()) {
    HOLOSCAN_LOG_INFO("Reached max_count={}, stopping reception", max_count_.get());
    receiver_->free_received_data(gpu_payload);
    return;
  }
  
  // Create shared_ptr with custom deleter that frees via facade
  auto receiver_ptr = receiver_.get();
  std::shared_ptr<void*> gpu_data_ptr(new void*(gpu_payload), 
    [receiver_ptr](void** ptr) {
      if (ptr != nullptr && *ptr != nullptr) {
        receiver_ptr->free_received_data(*ptr);
        *ptr = nullptr;
      }
      delete ptr;
    });
  
  // Create DLPack tensor descriptor using DLManagedTensorContext
  auto dl_context = std::make_shared<DLManagedTensorContext>();
  dl_context->memory_ref = gpu_data_ptr;
  
  // Setup shape (must live as long as the tensor)
  dl_context->dl_shape = {static_cast<int64_t>(payload_size)};
  
  // Setup DLTensor
  dl_context->tensor.dl_tensor.data = gpu_payload;
  dl_context->tensor.dl_tensor.device = DLDevice{kDLCUDA, gpu_device_.get()};
  dl_context->tensor.dl_tensor.ndim = 1;
  dl_context->tensor.dl_tensor.dtype = DLDataType{kDLUInt, 8, 1};  // uint8
  dl_context->tensor.dl_tensor.shape = dl_context->dl_shape.data();
  dl_context->tensor.dl_tensor.strides = nullptr;
  dl_context->tensor.dl_tensor.byte_offset = 0;
  
  // Create Holoscan Tensor from DLManagedTensorContext
  auto output_tensor = std::make_shared<holoscan::Tensor>(dl_context);
  
  HOLOSCAN_LOG_DEBUG("Received tensor: {} bytes (packet #{})",
                     payload_size, packets_received_);
  
  // Emit tensor
  op_output.emit(output_tensor, "tensor_out");
}

}  // namespace holoscan::ops
