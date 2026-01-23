# connext_ano_lib Test Suite

This directory contains unit, component, and integration tests for the `connext_ano_lib` library following COPILOT.md best practices.

## Test Categories

### Unit Tests (`connext_ano_unit_tests`)
Tests pure logic with no external dependencies. Execution time: <1 second.

**Configuration Validation** (`test_config_validation.cpp`):
- `SenderConfig` validation (11 tests): interface name, IP addresses, MAC addresses, UDP ports, header size, packet size constraints
- `ReceiverConfig` validation (7 tests): interface name, header size, packet size constraints, GPU device ID

**Network Utilities** (`test_network_utils.cpp`):
- MAC address parsing (7 tests): format validation, hex parsing, delimiter handling
- IPv4 address parsing (8 tests): format validation, octet range checking, network byte order
- Host byte order conversion (3 tests): endianness handling

**Packet Builder** (`test_packet_builder.cpp`):
- UDP/IPv4/Ethernet header construction (7 tests): field correctness, byte order, checksum offload, minimum packet size

### Component Tests (`connext_ano_component_tests`)
Tests CUDA-dependent functionality with real GPU resources. Execution time: ~0.3 seconds.

**CUDA Resource Manager** (`test_cuda_resource_manager.cpp`):
- Slot cycling behavior with wraparound
- Stream/event handle uniqueness across slots
- Buffer allocation and deallocation
- Asynchronous device-to-device copy with data integrity verification
- Readiness checking with async work

**RAII Wrappers** (`test_cuda_raii_wrappers.cpp`):
- `CudaStream`: construction, move semantics, destruction, implicit conversion
- `CudaEvent`: construction, move semantics, query methods, implicit conversion
- `CudaBuffer`: construction, move semantics, large allocations (100MB), data integrity

### Integration Tests
End-to-end tests requiring CUDA, DPDK, and physical network interfaces. Execution time: ~5 seconds (estimated).

**TX/RX Roundtrip** (`integration/test_tx_rx_roundtrip.cpp`):
- `test_tx_rx_loopback`: Software loopback testing (label: `integration;loopback`)
- `test_tx_rx_physical`: Physical network interface testing (label: `integration;requires_hardware`)

## Running Tests

### Prerequisites
```bash
# Build with testing enabled
./holohub build connext_ano_basic_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0 -DBUILD_TESTING:BOOL=ON"
```

### Execute Test Suites

**All tests**:
```bash
cd build/connext_ano_basic_app
ctest -V
```

**Unit tests only**:
```bash
ctest -R connext_ano_unit_tests -V
```

**Component tests only**:
```bash
ctest -R connext_ano_component_tests -V
```

**Integration tests**:
```bash
# Software loopback
ctest -R test_tx_rx_loopback -V

# Physical hardware (requires configured network interfaces)
ctest -R test_tx_rx_physical -V
```

**Exclude integration tests**:
```bash
ctest -LE integration -V
```

### Flakiness Testing
Component tests passed 10 consecutive iterations without failures (baseline established).

```bash
# Run component tests 10 times to detect flakiness
ctest -R connext_ano_component_tests --repeat until-fail:10
```

## Environment Variables

### `TEST_GPU_DEVICE`
Override the GPU device used for component and integration tests. Default: `0`.

```bash
export TEST_GPU_DEVICE=1
ctest -R connext_ano_component_tests -V
```

### Integration Test Environment Variables

Integration tests validate the end-to-end data path for GPU Direct network operations, ensuring packets can be transmitted from GPU memory and received back into GPU memory with proper payload verification.

#### Optional Configuration (Both Loopback and Physical Tests)

| Variable | Description | Default |
|----------|-------------|---------|
| `TEST_TX_IP` | Source IP address | `192.168.10.10` |
| `TEST_RX_IP` | Destination IP address | `192.168.10.11` |
| `TEST_ETH_DST_MAC` | Destination MAC address | `3c:6d:66:11:91:56` |

**Example - Custom network configuration for loopback test**:
```bash
export TEST_TX_IP="10.0.0.1"
export TEST_RX_IP="10.0.0.2"
export TEST_ETH_DST_MAC="aa:bb:cc:dd:ee:ff"
ctest -R test_tx_rx_loopback -V
```

#### Required Configuration (Physical NIC Test Only)

| Variable | Description | Example |
|----------|-------------|---------|
| `TEST_TX_NIC_PCIE` | PCIe address of TX NIC | `0005:03:00.0` |
| `TEST_RX_NIC_PCIE` | PCIe address of RX NIC | `0005:03:00.1` |
| `TEST_ETH_DST_MAC` or `TEST_RX_MAC`* | MAC address of RX NIC | `3c:6d:66:11:91:56` |

*Note: `TEST_RX_MAC` is supported for backward compatibility but `TEST_ETH_DST_MAC` is preferred.

**Example - Running physical NIC test with custom configuration**:
```bash
# Find NIC PCIe addresses
lspci | grep -i network

# Set required variables
export TEST_TX_NIC_PCIE="0005:03:00.0"
export TEST_RX_NIC_PCIE="0005:03:00.1"
export TEST_ETH_DST_MAC="3c:6d:66:11:91:56"

# Optional: customize network parameters
export TEST_TX_IP="192.168.20.10"
export TEST_RX_IP="192.168.20.11"

# Run the test
ctest -R test_tx_rx_physical -V
```

#### Test Architecture

Both tests follow the same pattern:
- Send 10 samples (1000 bytes each) from GPU memory
- Verify at least 8 packets received successfully (accounts for async transmission)
- Validate payload integrity using pattern matching
- Confirm DPDK statistics at both application and hardware level

The tests use a shared `RoundtripTestBase` class that provides:
- Common CUDA initialization
- Advanced Network manager configuration
- GPU memory region setup (separate TX/RX buffers with GPUDirect)
- Unified test execution logic with configurable flush phase

**Test Modes**:
- **Loopback Mode**: Software loopback using DPDK loopback interface (no hardware required)
- **Physical Hardware Mode**: Two physical NICs with GPUDirect RDMA capability (Mellanox ConnectX-6 or newer), connected with physical cable

## Test Results Summary

| Test Suite | Total Tests | Passing | Failing | Execution Time |
|------------|-------------|---------|---------|----------------|
| Unit Tests | 43 | 43 | 0 | <1 sec |
| Component Tests | 28 | 28 | 0 | 0.3 sec |
| Integration Tests | 2 | N/A | N/A | ~5 sec (est.) |

## Test Coverage

### What Each Category Validates

**Unit Tests** verify:
- Configuration structs reject invalid inputs (empty strings, out-of-range values)
- Network address parsing handles edge cases correctly
- Packet header construction follows protocol specifications
- Byte order conversions work correctly for network protocols

**Component Tests** verify:
- CUDA resource management cycles slots without leaks
- RAII wrappers correctly manage GPU resource lifetimes
- Async memory operations maintain data integrity
- Stream/event synchronization works correctly

**Integration Tests** verify:
- Complete TX/RX pipeline functions end-to-end
- DPDK integration works with real/emulated network interfaces
- GPU Direct RDMA paths operate correctly (physical hardware only)

## Architecture Decisions

### Test Isolation
- **Unit tests**: No shared state, no external dependencies, can run in parallel
- **Component tests**: Use `CudaTestFixture` with per-test GPU device setup/teardown, no resource locks (validated stable across 10 iterations)
- **Integration tests**: Separate executables for loopback vs. physical hardware to enable selective execution

### Shared Test Infrastructure
- `common/test_helpers.h`: Header-only helpers to avoid binary bloat
  - `CreateValidSenderConfig()` / `CreateValidReceiverConfig()`: Factory functions for valid test configurations
  - `CudaTestFixture`: Base fixture with GPU device initialization
  - `ASSERT_CUDA_SUCCESS()`: Macro for CUDA error checking with descriptive messages
  - Test constants: `kTestMacValid`, `kTestIpValid`, `kTestIpValidAlt`

### Build System
- Separate executables for each test category enable:
  - Selective execution (skip GPU tests on CPU-only systems)
  - Different timeout values (unit: 10s, component: 30s, integration: 30s)
  - Targeted dependency linking (unit tests don't link CUDA runtime)
  - Clear labeling for CI/CD filtering (future work)

## Future Enhancements (Not in Scope for Phase 1)

- CI/CD integration (GitHub Actions / GitLab CI)
- Code coverage measurement (gcov/lcov)
- Performance benchmarking for packet processing paths
- Stress testing with high-throughput scenarios
- Multi-GPU testing (TEST_GPU_DEVICE=0,1,2,3)
- Network fault injection for integration tests

---

## Legacy Documentation (Physical NIC Test Details)

### Prerequisites for Physical NIC Test

```bash
# Find NIC PCIe Addresses
lspci | grep -i network

# Get detailed info including MAC address
sudo lshw -C network -businfo
```

### Minimal Configuration (Legacy)
```bash
export TEST_TX_NIC_PCIE="0005:03:00.0"
export TEST_RX_NIC_PCIE="0005:03:00.1"
export TEST_RX_MAC="3c:6d:66:11:91:56"
ctest -R test_tx_rx_physical --verbose
```

### Expected Output (Loopback Test)
```
TX Statistics: packets=10, bytes=10000, dropped=0
RX Statistics: packets=10, bytes=10000, polls=40, empty_polls=30
Unique received samples: 10
[  PASSED  ] TxRxRoundtripTest.SendReceive10SamplesExpect8
```

### Expected Output (Physical NIC Test)
```
[Physical NIC] TX Statistics: packets=10, bytes=10000, dropped=0
[Physical NIC] RX Statistics: packets=9, bytes=9000, polls=40, empty_polls=31
[Physical NIC] Unique received samples: 9
Port 0 (tx_port): Transmitted 10 packets (10,640 bytes including headers)
Port 1 (rx_port): Received 10 packets (10,640 bytes)
[  PASSED  ] PhysicalNicRoundtripTest.SendReceive10SamplesExpect8
```

### Troubleshooting

**Test Fails with "TEST_RX_MAC environment variable not set"**:
```bash
export TEST_TX_NIC_PCIE="<your_tx_nic_pcie>"
export TEST_RX_NIC_PCIE="<your_rx_nic_pcie>"
export TEST_ETH_DST_MAC="<your_rx_nic_mac>"
```

**Test Receives Fewer Than 8 Packets**:
- Check network cable connection (physical NIC test)
- Verify NICs are in promiscuous mode
- Confirm MAC address configuration
- Check DPDK logs for initialization issues
- Verify NIC PCIe addresses: `lspci | grep -i network`
- Confirm MAC addresses: `ip link show`

**DPDK Initialization Fails** - Ensure proper permissions and huge pages:
```bash
# Check huge pages
cat /proc/meminfo | grep Huge

# Set huge pages (if needed)
echo 2048 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

### Test Implementation Details
- **Base Class Pattern:** `RoundtripTestBase` eliminates code duplication between loopback and physical tests
- **Automatic DPDK Worker Launch:** No explicit `mgr.run()` needed
- **GPU Memory Isolation:** Separate TX/RX memory regions with `owned=true`
- **Flush Phase for Physical NICs:** 500ms polling to ensure CUDA event completion
- **Flexible Configuration:** All network parameters configurable via environment variables
- **Backward Compatibility:** Supports legacy `TEST_RX_MAC` variable

See `integration/test_tx_rx_roundtrip.cpp` for detailed implementation (~605 lines, ~300 lines shared base class).

