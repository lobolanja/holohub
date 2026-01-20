# Connext Common Library

Shared utilities for Holoscan connext applications.

## Overview

This library provides reusable components for connext-based applications:

- **PacketBurstManager**: High-level manager for Advanced Network TX burst lifecycle
- **PacketBuilder**: Utilities for constructing network packet headers
- **CudaResourceManager**: Manages CUDA streams and events for async operations
- **NetworkUtils**: Utilities for parsing network addresses (IP and MAC)
- **NetworkConfig**: Configuration struct for network parameters

## Usage

### CMake Integration

Add the following to your application's `CMakeLists.txt`:

```cmake
target_link_libraries(your_application PRIVATE connext_common)
```

Make sure your application links against `holoscan::core` and the Advanced Network library.

### PacketBurstManager

The `PacketBurstManager` class provides a high-level abstraction for managing TX packet burst lifecycle with the Advanced Network library. It handles burst allocation, packet data population, CUDA synchronization, and transmission to the NIC.

#### Basic Usage Pattern

```cpp
#include <packet_burst_manager.h>
#include <packet_builder.h>

// 1. Initialize the manager
PacketBurstManager burst_mgr(
  port_id,           // DPDK port ID from get_port_id()
  queue_id,          // TX queue ID (e.g., 0)
  header_size,       // Size of packet headers (typically 42 bytes for Eth+IP+UDP)
  max_packet_size    // Maximum packet size including headers (e.g., 9000 for jumbo frames)
);

// 2. Create packet header template on GPU
void* gpu_header = nullptr;
cudaMalloc(&gpu_header, header_size);

// Build UDP/IPv4/Ethernet header
UDPIPV4Pkt header_template = PacketBuilder::create_udp_ipv4_packet(
  max_packet_size,
  src_port, dst_port,
  src_ip, dst_ip,
  dst_mac,
  64  // TTL
);

// Copy header template to GPU
cudaMemcpy(gpu_header, &header_template, sizeof(header_template), cudaMemcpyDefault);

// 3. In your compute loop:
cudaStream_t stream = /* your CUDA stream */;
void* payload_data = /* GPU pointer to your data */;
size_t payload_bytes = /* size of payload */;
int num_packets = 1;

// Allocate burst from DPDK pool
BurstParams* burst = burst_mgr.prepare_tx_burst(num_packets);
if (burst == nullptr) {
  // Burst not available, try again later
  return;
}

// Populate packet data (header + payload) on GPU
if (!burst_mgr.populate_tx_packet_data(burst, gpu_header, payload_data, 
                                       payload_bytes, num_packets, stream)) {
  // Population failed, burst is automatically cleaned up
  return;
}

// Get CUDA event for synchronization
cudaEvent_t event = /* your CUDA event */;
cudaEventRecord(event, stream);

// Enqueue burst for async transmission
burst_mgr.enqueue_tx_burst(burst, event);

// Send ready bursts to NIC (non-blocking)
int sent_count = burst_mgr.send_ready_bursts();
if (sent_count > 0) {
  HOLOSCAN_LOG_INFO("Sent {} bursts", sent_count);
}
```

#### API Reference

**Constructor**
```cpp
PacketBurstManager(int port_id, uint16_t queue_id, 
                   uint16_t header_size, uint16_t max_packet_size);
```

**Public Methods**
- `BurstParams* prepare_tx_burst(int num_packets)` - Allocates burst from DPDK pool
- `bool populate_tx_packet_data(burst, header_template, payload_data, payload_bytes, num_packets, stream)` - Copies header+payload to GPU packet buffers
- `void enqueue_tx_burst(burst, event)` - Queues burst for async transmission
- `int send_ready_bursts()` - Sends CUDA-completed bursts to NIC, returns count sent

**Utility Methods**
- `uint16_t header_size()` - Returns configured header size
- `uint16_t max_packet_size()` - Returns maximum packet size
- `uint16_t max_payload_size()` - Returns maximum payload size (max_packet_size - header_size)

#### Error Handling

All methods handle errors internally with logging:
- `prepare_tx_burst()` returns `nullptr` on failure
- `populate_tx_packet_data()` returns `false` and cleans up burst on failure
- `send_ready_bursts()` logs errors but continues processing other bursts

#### Thread Safety

**Not thread-safe**. Each instance should be used by a single thread only. Create separate instances for multi-threaded usage.

#### Best Practices

1. **Reuse header templates**: Create header template once and reuse across transmissions
2. **Batch processing**: Call `send_ready_bursts()` regularly to flush completed bursts
3. **Error recovery**: Check return values and handle throttling gracefully
4. **CUDA synchronization**: Always pass a valid CUDA stream and event
5. **Resource cleanup**: Manager automatically cleans up on destruction

### PacketBuilder

Static utility functions for constructing network packet headers.

#### API

```cpp
#include <packet_builder.h>

// Parse addresses first
std::array<uint8_t, 6> dst_mac;
NetworkUtils::parse_mac_address("AA:BB:CC:DD:EE:FF", dst_mac);
uint32_t src_ip = NetworkUtils::parse_ipv4_address("192.168.10.10");
uint32_t dst_ip = NetworkUtils::parse_ipv4_address("192.168.10.11");

// Create UDP/IPv4/Ethernet packet header
UDPIPV4Pkt header = PacketBuilder::create_udp_ipv4_packet(
  9000,      // max_packet_size
  5000,      // udp_src_port
  5001,      // udp_dst_port
  src_ip,    // ip_src (network byte order)
  dst_ip,    // ip_dst (network byte order)
  dst_mac,   // eth_dst MAC address
  64         // ip_ttl (optional, default: 64)
);

// Header is ready to use
// Source MAC and checksums are configured for hardware offload
```

#### Features

- **Complete headers**: Constructs Ethernet + IPv4 + UDP headers in one call
- **Hardware offload**: Configures checksums and source MAC for NIC offload
- **Network byte order**: Handles all byte order conversions automatically
- **Thread-safe**: Safe for concurrent calls from multiple threads

#### Thread Safety

**Thread-safe**. All functions can be called concurrently.

### CudaResourceManager

Manages multiple CUDA streams and events for concurrent async operations:

```cpp
#include <cuda_resource_manager.h>

// Create manager with 4 concurrent slots
CudaResourceManager cuda_mgr(4);

// Check if current slot is ready
if (cuda_mgr.is_ready()) {
  // Get stream for async operations
  cudaStream_t stream = cuda_mgr.get_stream();
  cudaMemcpyAsync(dst, src, size, cudaMemcpyDeviceToDevice, stream);
  
  // Record event and advance to next slot
  cuda_mgr.record_and_advance();
}
```

**Thread-safety**: Not thread-safe. Each instance should be used by a single thread only.

### NetworkUtils

Utilities for parsing network addresses:

```cpp
#include <network_utils.h>

// Parse MAC address
std::array<uint8_t, 6> mac;
NetworkUtils::parse_mac_address("AA:BB:CC:DD:EE:FF", mac);

// Parse IP address (network byte order)
uint32_t ip_net = NetworkUtils::parse_ipv4_address("192.168.1.1");

// Parse IP address (host byte order)
uint32_t ip_host = NetworkUtils::parse_ipv4_address_host_order("192.168.1.1");
```

**Thread-safety**: Thread-safe for concurrent calls.

**Error handling**: All functions throw `std::runtime_error` on invalid input.

## Complete Example

Here's a complete example showing how to use all components together in a Holoscan operator:

```cpp
#include <packet_burst_manager.h>
#include <packet_builder.h>
#include <cuda_resource_manager.h>
#include <network_utils.h>

class MyNetworkTxOp : public holoscan::Operator {
 public:
  void initialize() override {
    Operator::initialize();
    
    // Parse network configuration
    std::array<uint8_t, 6> dst_mac;
    NetworkUtils::parse_mac_address("AA:BB:CC:DD:EE:FF", dst_mac);
    uint32_t src_ip = NetworkUtils::parse_ipv4_address("192.168.10.10");
    uint32_t dst_ip = NetworkUtils::parse_ipv4_address("192.168.10.11");
    
    // Get port ID from Advanced Network
    port_id_ = get_port_id("eth0");
    
    // Initialize CUDA resource manager (4 concurrent slots)
    cuda_mgr_ = std::make_unique<CudaResourceManager>(4);
    
    // Initialize burst manager
    burst_mgr_ = std::make_unique<PacketBurstManager>(
      port_id_, 0, 42, 9000  // port, queue, header_size, max_packet_size
    );
    
    // Create GPU header template
    cudaMalloc(&gpu_header_, 42);
    UDPIPV4Pkt header = PacketBuilder::create_udp_ipv4_packet(
      9000, 5000, 5001, src_ip, dst_ip, dst_mac
    );
    cudaMemcpy(gpu_header_, &header, sizeof(header), cudaMemcpyDefault);
  }
  
  void compute(InputContext& input, OutputContext& output, ExecutionContext& ctx) override {
    // Check if CUDA resources are ready
    if (!cuda_mgr_->is_ready()) {
      return;  // Previous batch still in flight
    }
    
    // Get input data
    auto tensor = input.receive<std::shared_ptr<holoscan::Tensor>>("input");
    if (!tensor) return;
    
    void* gpu_data = tensor.value()->data();
    size_t data_bytes = tensor.value()->nbytes();
    
    // Prepare TX burst
    BurstParams* burst = burst_mgr_->prepare_tx_burst(1);
    if (!burst) return;
    
    // Populate packet data
    cudaStream_t stream = cuda_mgr_->get_stream();
    if (!burst_mgr_->populate_tx_packet_data(
          burst, gpu_header_, gpu_data, data_bytes, 1, stream)) {
      return;
    }
    
    // Enqueue for async transmission
    cudaEvent_t event = cuda_mgr_->get_event();
    cuda_mgr_->record_and_advance();
    burst_mgr_->enqueue_tx_burst(burst, event);
    
    // Send ready bursts
    burst_mgr_->send_ready_bursts();
  }
  
  ~MyNetworkTxOp() {
    if (gpu_header_) cudaFree(gpu_header_);
  }
  
 private:
  int port_id_;
  void* gpu_header_ = nullptr;
  std::unique_ptr<CudaResourceManager> cuda_mgr_;
  std::unique_ptr<PacketBurstManager> burst_mgr_;
};
```

## Requirements

- C++17 or later
- CUDA Toolkit 11.0+
- Holoscan SDK 3.0+
- Advanced Network library (DPDK or Rivermax backend)

## Additional Resources

### NetworkConfig

For backward compatibility, the library still provides `NetworkConfig` struct:

```cpp
#include <network_utils.h>

NetworkConfig config;
config.interface_name = "eth0";
config.ip_src_addr = "192.168.1.100";
config.ip_dst_addr = "192.168.1.200";
config.eth_dst_addr = "AA:BB:CC:DD:EE:FF";
config.udp_src_port = 5000;
config.udp_dst_port = 5001;
config.header_size = 42;
config.max_packet_size = 9000;
config.batch_size = 64;
config.header_data_split = 0;  // 0 = GPU-only mode
```

### See Also

- `tensor_network_tx_op.cpp` - Reference implementation using PacketBurstManager
- Advanced Network documentation - For details on BurstParams and DPDK configuration

## License

SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
