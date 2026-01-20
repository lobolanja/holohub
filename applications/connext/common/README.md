# Connext Common Library

Shared utilities for Holoscan connext applications.

## Overview

This library provides reusable components for connext-based applications:

- **CudaResourceManager**: Manages CUDA streams and events for async operations
- **NetworkUtils**: Utilities for parsing network addresses (IP and MAC)
- **NetworkConfig**: Configuration struct for network parameters

## Usage

### CMake Integration

Add the following to your application's `CMakeLists.txt`:

```cmake
target_link_libraries(your_application PRIVATE connext_common)
```

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

### NetworkConfig

Configuration struct for network parameters:

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

## Requirements

- C++17 or later
- CUDA Toolkit
- fmt library (for error messages)

## License

SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0
