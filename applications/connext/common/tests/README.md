# ConnExt GPU Direct Network TX/RX Tests

Integration tests for GPU Direct network transmission and reception using DPDK and GPUDirect RDMA.

## Overview

This test suite validates the end-to-end data path for GPU Direct network operations, ensuring packets can be transmitted from GPU memory and received back into GPU memory with proper payload verification.

### Test Fixtures

1. **TxRxRoundtripTest (Loopback Mode)**
   - Tests software loopback using DPDK loopback interface
   - No hardware requirements
   - Ideal for CI/CD pipelines and development

2. **PhysicalNicRoundtripTest (Physical Hardware Mode)**
   - Tests actual hardware data path using two physical NICs
   - Requires GPUDirect RDMA capable NICs (Mellanox ConnectX-6 or newer)
   - Physical cable connecting the two NICs
   - Validates real-world network performance

## Test Architecture

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

## Environment Variables

### Optional Configuration (Both Tests)

| Variable | Description | Default |
|----------|-------------|---------|
| `TEST_TX_IP` | Source IP address | `192.168.10.10` |
| `TEST_RX_IP` | Destination IP address | `192.168.10.11` |
| `TEST_ETH_DST_MAC` | Destination MAC address | `3c:6d:66:11:91:56` |

### Required Configuration (Physical NIC Test Only)

| Variable | Description | Example |
|----------|-------------|---------|
| `TEST_TX_NIC_PCIE` | PCIe address of TX NIC | `0005:03:00.0` |
| `TEST_RX_NIC_PCIE` | PCIe address of RX NIC | `0005:03:00.1` |
| `TEST_ETH_DST_MAC` or `TEST_RX_MAC`* | MAC address of RX NIC | `3c:6d:66:11:91:56` |

*Note: `TEST_RX_MAC` is supported for backward compatibility but `TEST_ETH_DST_MAC` is preferred.

## Running Tests

### Prerequisites

```bash
# Navigate to build directory
cd /workspace/holohub/build/connext_app_cpp

# Ensure tests are built
ninja test_tx_rx_loopback test_tx_rx_physical
```

### Loopback Test (No Hardware Required)

#### Default Configuration
```bash
ctest -R test_tx_rx_loopback --verbose
```

#### Custom Network Configuration
```bash
export TEST_TX_IP="10.0.0.1"
export TEST_RX_IP="10.0.0.2"
export TEST_ETH_DST_MAC="aa:bb:cc:dd:ee:ff"
ctest -R test_tx_rx_loopback --verbose
```

### Physical NIC Test (Hardware Required)

#### Find NIC PCIe Addresses
```bash
# List all network devices
lspci | grep -i network

# Get detailed info including MAC address
sudo lshw -C network -businfo
```

#### Minimal Configuration (Legacy)
```bash
export TEST_TX_NIC_PCIE="0005:03:00.0"
export TEST_RX_NIC_PCIE="0005:03:00.1"
export TEST_RX_MAC="3c:6d:66:11:91:56"
ctest -R test_tx_rx_physical --verbose
```

#### Full Custom Configuration
```bash
export TEST_TX_NIC_PCIE="0005:03:00.0"
export TEST_RX_NIC_PCIE="0005:03:00.1"
export TEST_ETH_DST_MAC="3c:6d:66:11:91:56"
export TEST_TX_IP="192.168.20.10"
export TEST_RX_IP="192.168.20.11"
ctest -R test_tx_rx_physical --verbose
```

### Run All Tests
```bash
# Set environment for physical NIC test
export TEST_TX_NIC_PCIE="0005:03:00.0"
export TEST_RX_NIC_PCIE="0005:03:00.1"
export TEST_ETH_DST_MAC="3c:6d:66:11:91:56"

# Run all connext tests
ctest -R test_tx_rx --verbose
```

## Expected Output

### Successful Test Output
```
TX Statistics: packets=10, bytes=10000, dropped=0
RX Statistics: packets=10, bytes=10000, polls=40, empty_polls=30
Unique received samples: 10
[  PASSED  ] TxRxRoundtripTest.SendReceive10SamplesExpect8
```

### Physical NIC Test Output
```
[Physical NIC] TX Statistics: packets=10, bytes=10000, dropped=0
[Physical NIC] RX Statistics: packets=9, bytes=9000, polls=40, empty_polls=31
[Physical NIC] Unique received samples: 9
Port 0 (tx_port): Transmitted 10 packets (10,640 bytes including headers)
Port 1 (rx_port): Received 10 packets (10,640 bytes)
[  PASSED  ] PhysicalNicRoundtripTest.SendReceive10SamplesExpect8
```

## Troubleshooting

### Test Fails with "TEST_RX_MAC environment variable not set"
**Solution:** Set required environment variables for physical NIC test:
```bash
export TEST_TX_NIC_PCIE="<your_tx_nic_pcie>"
export TEST_RX_NIC_PCIE="<your_rx_nic_pcie>"
export TEST_ETH_DST_MAC="<your_rx_nic_mac>"
```

### Test Receives Fewer Than 8 Packets
**Possible causes:**
- Network cable not properly connected (physical NIC test)
- NICs not in promiscuous mode
- Incorrect MAC address configuration
- DPDK initialization issues

**Debug steps:**
1. Check DPDK logs for "Creating dummy queue" messages
2. Verify NIC PCIe addresses: `lspci | grep -i network`
3. Confirm MAC addresses: `ip link show`
4. Increase test timeout if needed (currently 30s)

### DPDK Initialization Fails
**Solution:** Ensure proper permissions and huge pages:
```bash
# Check huge pages
cat /proc/meminfo | grep Huge

# Set huge pages (if needed)
echo 2048 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

## Test Implementation Details

### Key Features
- **Base Class Pattern:** `RoundtripTestBase` eliminates code duplication
- **Automatic DPDK Worker Launch:** No explicit `mgr.run()` needed
- **Automatic Dummy Queues:** DPDK creates `UNUSED` queues as needed
- **GPU Memory Isolation:** Separate TX/RX memory regions with `owned=true`
- **Flush Phase for Physical NICs:** 500ms polling to ensure CUDA event completion
- **Flexible Configuration:** All network parameters configurable via environment variables

### Code Statistics
- **File:** `test_tx_rx_roundtrip.cpp`
- **Lines:** ~605 (refactored from 710, 15% reduction)
- **Test Execution Time:** ~4-5 seconds per test
- **Code Reuse:** ~300 lines of common logic in base class

## Contributing

When modifying tests:
1. Maintain backward compatibility with `TEST_RX_MAC`
2. Keep both tests in sync (same payload patterns, timing, expectations)
3. Update this README if adding new environment variables
4. Run both tests before committing changes

## References

- [Holoscan Advanced Network Operator Documentation](https://docs.nvidia.com/holoscan/)
- [DPDK Programming Guide](https://doc.dpdk.org/guides/prog_guide/)
- [GPUDirect RDMA](https://docs.nvidia.com/cuda/gpudirect-rdma/)
