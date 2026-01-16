/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tensor_network_rx_op.h"

namespace holoscan::ops {

TensorNetworkRxOp::~TensorNetworkRxOp() {
  HOLOSCAN_LOG_INFO("TensorNetworkRxOp shutting down. Received {}/{} packets/bytes",
                    packets_received_, bytes_received_);
  
  // Cleanup CUDA resources
  for (int i = 0; i < num_concurrent; i++) {
    if (streams_[i]) cudaStreamDestroy(streams_[i]);
    if (events_[i]) cudaEventDestroy(events_[i]);
  }
  
  // Free any pending batches
  while (!batch_q_.empty()) {
    free_all_packets_and_burst_rx(batch_q_.front().burst);
    batch_q_.pop();
  }
}

void TensorNetworkRxOp::setup(OperatorSpec& spec) {
  spec.output<std::shared_ptr<holoscan::Tensor>>("tensor_out");
  
  spec.param<std::string>(interface_name_, "interface_name", "Interface Name",
             "Name of NIC interface from advanced_network config");
  spec.param<int>(hds_, "split_boundary", "Header-Data Split",
             "Header-data split boundary (0 for GPU-only)", 0);
  spec.param<uint32_t>(batch_size_, "batch_size", "Batch Size",
             "Batch size in packets for each processing epoch", 64);
  spec.param<uint16_t>(max_packet_size_, "max_packet_size", "Max Packet Size",
             "Maximum packet size expected from sender", 9000);
  spec.param<uint16_t>(header_size_, "header_size", "Header Size",
             "Header size on each packet from L4 and below", 42);
  spec.param<int>(gpu_device_, "gpu_device", "GPU Device",
             "GPU device ID", 0);
}

void TensorNetworkRxOp::initialize() {
  HOLOSCAN_LOG_INFO("TensorNetworkRxOp::initialize()");
  holoscan::Operator::initialize();

  // Get port ID from advanced_network
  port_id_ = get_port_id(interface_name_.get());
  if (port_id_ == -1) {
    throw std::runtime_error(
        fmt::format("Invalid interface '{}' specified", interface_name_.get()));
  }

  // Set GPU device
  cudaSetDevice(gpu_device_.get());

  // Create CUDA streams and events
  for (int i = 0; i < num_concurrent; i++) {
    cudaStreamCreate(&streams_[i]);
    cudaEventCreate(&events_[i]);
  }

  HOLOSCAN_LOG_INFO("TensorNetworkRxOp initialized: port_id={}, GPU-only mode={}",
                    port_id_, !hds_.get());
}

void TensorNetworkRxOp::free_processed_packets() {
  // Free batches that have completed CUDA processing
  while (!batch_q_.empty()) {
    const auto& batch = batch_q_.front();
    if (cudaEventQuery(batch.evt) == cudaSuccess) {
      free_all_packets_and_burst_rx(batch.burst);
      batch_q_.pop();
    } else {
      break;  // No need to check further if this one isn't done
    }
  }
}

void TensorNetworkRxOp::compute(InputContext& op_input, OutputContext& op_output,
                                ExecutionContext& context) {
  // Free any previously processed packets
  free_processed_packets();

  BurstParams* burst = nullptr;
  
  // Iterate over all RX queues like the benchmark does
  const auto num_rx_queues = get_num_rx_queues(port_id_);
  
  static int log_counter = 0;
  static bool logged_num_queues = false;
  
  if (!logged_num_queues) {
    HOLOSCAN_LOG_INFO("Port {} has {} RX queues", port_id_, num_rx_queues);
    logged_num_queues = true;
  }
  
  log_counter++;
  
  for (int q = 0; q < num_rx_queues; q++) {
    Status status = get_rx_burst(&burst, port_id_, q);
    
    if (log_counter % 10000 == 0) {
      HOLOSCAN_LOG_INFO("Polling port {} queue {} (attempt {}, status={})", 
                        port_id_, q, log_counter, (int)status);
    }
    
    if (status != Status::SUCCESS) {
      if (status != Status::NOT_READY && log_counter % 10000 == 0) {
        HOLOSCAN_LOG_WARN("Queue {}: Unexpected status: {}", q, (int)status);
      }
      continue;  // Try next queue
    }
  
  if (burst == nullptr) {
    if (log_counter % 10000 == 0) {
      HOLOSCAN_LOG_WARN("Queue {}: Status SUCCESS but burst is nullptr!", q);
    }
    continue;  // Try next queue
  }

  auto burst_size = get_num_packets(burst);
  if (burst_size == 0) {
    free_all_packets_and_burst_rx(burst);
    continue;  // Try next queue
  }
  
  // If we got here, we received packets!
  HOLOSCAN_LOG_INFO("✓ Received {} packets from queue {}!", burst_size, q);

  packets_received_ += burst_size;

  // For simplicity, assume single packet contains one tensor
  // In production, you'd reassemble from multiple packets
  
  // Get first packet
  void* gpu_pkt_ptr = get_packet_ptr(burst, 0);
  uint16_t pkt_len = get_packet_length(burst, 0);
  
  // Calculate payload size (skip headers)
  if (pkt_len <= header_size_.get()) {
    HOLOSCAN_LOG_WARN("Queue {}: Received packet too small: {} bytes", q, pkt_len);
    free_all_packets_and_burst_rx(burst);
    continue;  // Try next queue
  }
  
  size_t payload_size = pkt_len - header_size_.get();
  bytes_received_ += payload_size;
  
  // Allocate GPU memory for tensor (copying payload without headers)
  void* tensor_gpu_data = nullptr;
  cudaError_t cuda_result = cudaMalloc(&tensor_gpu_data, payload_size);
  if (cuda_result != cudaSuccess) {
    HOLOSCAN_LOG_ERROR("Failed to allocate GPU memory: {}", cudaGetErrorString(cuda_result));
    free_all_packets_and_burst_rx(burst);
    break;  // Critical error, exit loop completely
  }
  
  // Copy payload (skip header) from packet buffer to tensor buffer
  void* payload_ptr = static_cast<uint8_t*>(gpu_pkt_ptr) + header_size_.get();
  cudaMemcpyAsync(tensor_gpu_data, payload_ptr, payload_size,
                  cudaMemcpyDeviceToDevice, streams_[cur_batch_idx_]);
  
  // Record event
  cudaEventRecord(events_[cur_batch_idx_], streams_[cur_batch_idx_]);
  
  // Queue burst for later freeing (after CUDA completes)
  batch_q_.push(RxBatch{burst, events_[cur_batch_idx_]});
  
  // Create shared_ptr with custom deleter for GPU memory management
  std::shared_ptr<void*> gpu_data_ptr(new void*(tensor_gpu_data), [](void** ptr) {
    if (ptr != nullptr) {
      if (*ptr != nullptr) {
        cudaFree(*ptr);
        *ptr = nullptr;
      }
      delete ptr;
      ptr = nullptr;
    }
  });
  
  // Create DLPack tensor descriptor using DLManagedTensorContext
  auto dl_context = std::make_shared<DLManagedTensorContext>();
  dl_context->memory_ref = gpu_data_ptr;
  
  // Setup shape (must live as long as the tensor)
  dl_context->dl_shape = {static_cast<int64_t>(payload_size)};
  
  // Setup DLTensor
  dl_context->tensor.dl_tensor.data = tensor_gpu_data;
  dl_context->tensor.dl_tensor.device = DLDevice{kDLCUDA, gpu_device_.get()};
  dl_context->tensor.dl_tensor.ndim = 1;
  dl_context->tensor.dl_tensor.dtype = DLDataType{kDLUInt, 8, 1};  // uint8
  dl_context->tensor.dl_tensor.shape = dl_context->dl_shape.data();
  dl_context->tensor.dl_tensor.strides = nullptr;
  dl_context->tensor.dl_tensor.byte_offset = 0;
  
  // Create Holoscan Tensor from DLManagedTensorContext
  auto output_tensor = std::make_shared<holoscan::Tensor>(dl_context);
  
  HOLOSCAN_LOG_DEBUG("Received tensor: {} bytes from {} packets",
                     payload_size, burst_size);
  
  // Emit tensor
  op_output.emit(output_tensor, "tensor_out");
  
  // Move to next concurrent slot
  cur_batch_idx_ = (cur_batch_idx_ + 1) % num_concurrent;
  
  // Found packets in this queue, exit loop
  break;
  
  }  // End of queue loop
}

}  // namespace holoscan::ops
