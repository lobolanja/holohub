# Connext Common Library

Shared utilities for Holoscan connext applications with GPU Direct networking.

## Overview

This library provides a high-level facade for GPU Direct network transmission using DPDK and CUDA, abstracting away low-level packet construction and hardware details.

## Public API

### GPU Direct Network Sender

The main public interface is `IGpuDirectNetworkSender`, a facade that encapsulates all CUDA, DPDK, and packet construction details.

#### Features

- **Zero-copy transmission**: GPU → NIC via GPUDirect (no CPU involvement)
- **Simple API**: Single `send()` call with void* GPU pointer
- **Automatic packet construction**: Ethernet + IP + UDP headers built automatically
- **Async CUDA operations**: Event-based flow control for optimal throughput
- **Statistics tracking**: Packets sent, bytes transmitted, frames dropped
- **Exception-based errors**: Clear exception hierarchy for error handling

#### Basic Usage

```cpp
#include <gpu_direct_network_sender.h>
#include <sender_config.h>
#include <gpu_direct_exceptions.h>

// 1. Configure the sender
SenderConfig config;
config.interface_name = "loopback_ports";  // From advanced_network config
config.queue_id = 0;                        // TX queue ID
config.ip_src_addr = "192.168.10.10";
config.ip_dst_addr = "192.168.10.11";
config.eth_dst_addr = "3c:6d:66:11:91:56"; // Destination MAC
config.udp_src_port = 4096;
config.udp_dst_port = 4096;
config.header_size = 64;                    // Including padding
config.max_packet_size = 1064;              // Total packet size

// Validate configuration (throws InvalidConfigException)
config.validate();

// 2. Create sender instance (throws on initialization errors)
auto sender = IGpuDirectNetworkSender::create(config);

// 3. Transmit data in your compute loop
void* gpu_data = /* your CUDA device pointer */;
size_t data_size = 1000;  // Bytes to send

try {
  if (sender->is_ready()) {
    sender->send(gpu_data, data_size);
    HOLOSCAN_LOG_INFO("✓ Sent {} bytes", data_size);
  } else {
    // Previous batch still in flight, skip this frame
    HOLOSCAN_LOG_DEBUG("Skipping frame - sender not ready");
  }
} catch (const NotReadyException& e) {
  HOLOSCAN_LOG_ERROR("Send failed: {}", e.what());
}

// 4. Check statistics
auto stats = sender->get_stats();
HOLOSCAN_LOG_INFO("Total: {} packets, {} bytes, {} dropped",
                  stats.packets_sent, stats.bytes_transmitted, stats.frames_dropped);
```

#### API Reference

**Configuration**
```cpp
struct SenderConfig {
  std::string interface_name;  // NIC interface from advanced_network config
  uint16_t queue_id;           // TX queue ID (must match advanced_network)
  std::string ip_src_addr;     // Source IPv4 (e.g., "192.168.10.10")
  std::string ip_dst_addr;     // Destination IPv4 (e.g., "192.168.10.11")
  std::string eth_dst_addr;    // Destination MAC (e.g., "AA:BB:CC:DD:EE:FF")
  uint16_t udp_src_port;       // UDP source port
  uint16_t udp_dst_port;       // UDP destination port
  uint16_t header_size;        // Header size including padding (≥42)
  uint16_t max_packet_size;    // Maximum packet size including headers
  
  void validate();  // Throws InvalidConfigException if invalid
};
```

**Sender Interface**
```cpp
class IGpuDirectNetworkSender {
 public:
  // Send GPU buffer to network (silently truncates if size > max_payload_size)
  virtual void send(void* gpu_data, size_t size) = 0;
  
  // Check if ready for transmission (previous batch completed)
  virtual bool is_ready() const = 0;
  
  // Get maximum payload size (max_packet_size - header_size)
  virtual size_t max_payload_size() const = 0;
  
  // Get cumulative statistics
  virtual TransmissionStats get_stats() const = 0;
  
  // Reset statistics counters
  virtual void reset_stats() = 0;
  
  // Factory method to create sender
  static std::unique_ptr<IGpuDirectNetworkSender> create(const SenderConfig& config);
};
```

**Statistics**
```cpp
struct TransmissionStats {
  uint64_t packets_sent;        // Total packets transmitted
  uint64_t bytes_transmitted;   // Total payload bytes transmitted
  uint64_t frames_dropped;      // Frames skipped due to not-ready state
};
```

#### Exception Hierarchy

```cpp
// Base exception
class GpuDirectException : public std::runtime_error;

// Configuration errors (thrown during validation or create())
class InvalidConfigException : public GpuDirectException;

// Network initialization errors (thrown during create())
class NetworkInitException : public GpuDirectException;

// CUDA errors (thrown during create() or send())
class CudaInitException : public GpuDirectException;

// Runtime errors (thrown during send())
class NotReadyException : public GpuDirectException {
  size_t batch_index;  // Which batch is still in flight
};
```

#### Thread Safety

**NOT thread-safe**. Use each sender instance from a single thread only.

#### GPU-Only Mode

The sender operates in **GPU-only mode** (no header-data split):
- Entire packet (headers + payload) resides on GPU
- Zero CPU copies during transmission
- Requires DPDK GPUDirect support

#### Configuration Constraints

- `header_size` must be ≥ 42 bytes (Ethernet + IP + UDP minimum)
- IP TTL is hardcoded to 64
- Concurrent batches internally set to 4
- Payload truncated if `size > max_payload_size()`

### Integration with Holoscan Operators

```cpp
class TensorNetworkTxOp : public Operator {
 public:
  void initialize() override {
    // Build configuration from operator parameters
    SenderConfig config;
    config.interface_name = interface_name_.get();
    config.queue_id = queue_id_.get();
    config.ip_src_addr = ip_src_addr_.get();
    config.ip_dst_addr = ip_dst_addr_.get();
    config.eth_dst_addr = eth_dst_addr_.get();
    config.udp_src_port = udp_src_port_.get();
    config.udp_dst_port = udp_dst_port_.get();
    config.header_size = header_size_.get();
    config.max_packet_size = max_packet_size_.get();
    
    try {
      config.validate();
      sender_ = IGpuDirectNetworkSender::create(config);
      HOLOSCAN_LOG_INFO("Sender initialized: max_payload={} bytes", 
                        sender_->max_payload_size());
    } catch (const GpuDirectException& e) {
      HOLOSCAN_LOG_ERROR("Failed to initialize sender: {}", e.what());
      throw;
    }
  }
  
  void compute(InputContext& input, OutputContext&, ExecutionContext&) override {
    // Receive tensor from input port
    auto tensor = input.receive<std::shared_ptr<Tensor>>("tensor_in").value();
    
    // Check readiness
    if (!sender_->is_ready()) {
      HOLOSCAN_LOG_DEBUG("Sender not ready, skipping frame");
      auto stats = sender_->get_stats();
      stats.frames_dropped++;  // Track manually if needed
      return;
    }
    
    // Send GPU data directly
    void* gpu_ptr = tensor->data();
    size_t bytes = std::min(tensor->nbytes(), sender_->max_payload_size());
    
    try {
      sender_->send(gpu_ptr, bytes);
    } catch (const NotReadyException& e) {
      HOLOSCAN_LOG_ERROR("Send failed: {}", e.what());
    } catch (const CudaInitException& e) {
      HOLOSCAN_LOG_ERROR("CUDA error: {}", e.what());
    }
  }
  
 private:
  std::unique_ptr<IGpuDirectNetworkSender> sender_;
  Parameter<std::string> interface_name_;
  Parameter<uint16_t> queue_id_;
  // ... other parameters
};
```

## Requirements

- C++17 or later
- CUDA Toolkit 11.0+
- Holoscan SDK 3.0+
- Advanced Network library with DPDK backend
- GPUDirect support (DPDK GPU driver)

## CMake Integration

Add to your application's `CMakeLists.txt`:

```cmake
target_link_libraries(your_application PRIVATE connext_common)
```

The library automatically provides:
- Public headers: `gpu_direct_network_sender.h`, `sender_config.h`, `gpu_direct_exceptions.h`
- Dependencies: CUDA runtime, Holoscan core, Advanced Network

## Examples

### Complete TX Operator

See `applications/connext/connext_ano_basic_app/` for complete working examples:
- `tensor_network_tx_op.cpp` - TX operator using GPU Direct sender
- `demo_ano_tx.yaml` - Configuration example
- `tests/test_demo_ano.py` - Integration tests

## License

SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
