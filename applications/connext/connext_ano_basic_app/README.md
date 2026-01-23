# Connext Advanced Network Basic Demo

Demo application showing GPU-to-GPU communication using Holoscan's Advanced Network library with DPDK and GPUDirect.

## Overview

This application demonstrates GPU-to-GPU communication with three executables:

1. **demo_ano_tx_rx**: Combined TX+RX in a single DPDK process (recommended for testing)
2. **connext_ano_basic_app (tx mode)**: Generates tensors with "hello world #N" on GPU and transmits via network
3. **connext_ano_basic_app (rx mode)**: Receives tensors from network on GPU and prints contents

All modes use **GPU-only mode** (no header-data split) for maximum throughput with GPUDirect.

## Architecture

### Combined TX+RX Pipeline (demo_ano_tx_rx)
```
TensorGeneratorOp -> TensorNetworkTxOp -> Network (DPDK + GPUDirect)
                                       -> TensorNetworkRxOp -> TensorPrinterOp
```

### Separate TX Pipeline (connext_ano_basic_app tx)
```
TensorGeneratorOp -> TensorNetworkTxOp -> Network (DPDK + GPUDirect)
```

### Separate RX Pipeline (connext_ano_basic_app rx)
```
Network (DPDK + GPUDirect) -> TensorNetworkRxOp -> TensorPrinterOp
```

## Prerequisites

1. **DPDK-compatible NIC** (e.g., Mellanox ConnectX-5/6/7)
2. **GPUDirect** enabled
3. **System configuration** from [High Performance Networking tutorial](/tutorials/high_performance_networking/README.md)
4. **Single machine** (for loopback/combined mode) or **two machines (or docker instances)** (for separate TX/RX)

## Configuration Files

The application includes four YAML configuration files:

- **`demo_ano_tx_rx.yaml`**: Combined TX+RX on two separate NICs
- **`demo_ano_tx_rx_loopback.yaml`**: Combined TX+RX on single NIC (loopback mode)
- **`demo_ano_tx.yaml`**: TX only (for separate processes)
- **`demo_ano_rx.yaml`**: RX only (for separate processes)

### Configuration Parameters

Edit the YAML files with your network details:

#### For `demo_ano_tx_rx.yaml` and `demo_ano_tx_rx_loopback.yaml`:
- `<CHANGE_ME_TX_PCI_ADDRESS>`: PCIe address of TX NIC (e.g., `0005:03:00.0`)
- `<CHANGE_ME_RX_PCI_ADDRESS>`: PCIe address of RX NIC (e.g., `0005:03:00.1`)
- `<CHANGE_ME_TX_IP>`: TX machine IP address (e.g., `192.168.10.10`)
- `<CHANGE_ME_RX_IP>`: RX machine IP address (e.g., `192.168.10.11`)
- `<CHANGE_ME_RX_MAC>`: RX machine MAC address (e.g., `3c:6d:66:11:91:56`)

#### For `demo_ano_tx.yaml`:
- `<CHANGE_ME_TX_PCI_ADDRESS>`: PCIe address of TX NIC
- `<CHANGE_ME_TX_IP>`: TX machine IP address
- `<CHANGE_ME_RX_IP>`: RX machine IP address
- `<CHANGE_ME_RX_MAC>`: RX machine MAC address

#### For `demo_ano_rx.yaml`:
- `<CHANGE_ME_RX_PCI_ADDRESS>`: PCIe address of RX NIC

### Getting Configuration Values

```bash
# Get PCIe address of your NIC
lspci | grep -i mellanox  # or Ethernet/Network

# Get IP address
ip addr show

# Get MAC address
ip link show <interface_name>
```

## Build

Build the application from the HoloHub root:

```bash
# For local builds (no container)
./holohub build connext_ano_basic_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0"

# For container builds (required for separate TX/RX modes)
./holohub build connext_ano_basic_app --build-type debug \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0"
```

> **Note**: Adjust `CONNEXTDDS_ARCH` for your platform:
> - ARM64 (IGX, Jetson): `armv8Linux4gcc7.3.0`
> - x86_64: `x64Linux4gcc7.3.0`

This builds both executables:
- `demo_ano_tx_rx` (combined TX+RX)
- `connext_ano_basic_app` (separate TX or RX)

## Run

The application supports four modes via the HoloHub CLI. Configuration files are automatically copied to the build directory.

> **Important**: 
> - **Combined modes** (`tx_rx`, `tx_rx_loopback`): Can use `--local` flag (single DPDK process)
> - **Separate modes** (`tx`, `rx`): Must run in Docker containers (no `--local` flag) because DPDK can only be initialized once when running in the same host.

### View Available Modes

```bash
./holohub modes connext_ano_basic_app
```

### Mode 1: Combined TX+RX (Recommended for Testing)

Run both TX and RX in a single process - ideal for testing on one machine:

```bash
# With two NICs
./holohub run connext_ano_basic_app tx_rx --local

# Loopback mode (single NIC)
./holohub run connext_ano_basic_app tx_rx_loopback --local
```

### Mode 2: Separate TX and RX Processes

For distributed setups across two machines or when running in separate containers:

> **Note**: When running TX and RX separately, you **cannot use `--local`** because DPDK can only be initialized once per container/host. Each must run in its own Docker container with special DPDK-required privileges.

**Required Docker Options for DPDK:**
```bash
--docker-opts="--cap-add=SYS_ADMIN --cap-add=IPC_LOCK --cap-add=NET_ADMIN \
  --device=/dev/hugepages:/dev/hugepages --ulimit memlock=-1:-1 --privileged \
  -v /dev/hugepages:/dev/hugepages"
```

**Optional: RTI License (if using RTI Connext DDS):**
```bash
--docker-opts="-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat"
```

**On RX Machine (start first):**
```bash
./holohub run connext_ano_basic_app rx \
  --docker-opts="-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat \
    --cap-add=SYS_ADMIN --cap-add=IPC_LOCK --cap-add=NET_ADMIN \
    --device=/dev/hugepages:/dev/hugepages --ulimit memlock=-1:-1 --privileged \
    -v /dev/hugepages:/dev/hugepages"
```

**On TX Machine (or separate terminal):**
```bash
./holohub run connext_ano_basic_app tx \
  --docker-opts="-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat \
    --cap-add=SYS_ADMIN --cap-add=IPC_LOCK --cap-add=NET_ADMIN \
    --device=/dev/hugepages:/dev/hugepages --ulimit memlock=-1:-1 --privileged \
    -v /dev/hugepages:/dev/hugepages"
```

If running on the same machine, use separate terminals and ensure each runs in its own container (without `--local` flag).

### Default Mode

Running without specifying a mode uses `tx_rx` (combined) by default:
```bash
./holohub run connext_ano_basic_app --local
```

## Testing

The application includes automated tests in the `tests/` directory. Tests verify the combined TX+RX loopback functionality.

### Running Tests

Tests are built and run using CTest when `BUILD_TESTING` is enabled:

```bash
# Build with testing enabled
./holohub build connext_ano_basic_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0" \
  --configure-args="-DBUILD_TESTING:BOOL=ON"

# Run all tests
cd build/connext_ano_basic_app/applications/connext/connext_ano_basic_app
ctest --output-on-failure

# Or run tests with verbose output
ctest -V

# Run a specific test
ctest -R test_demo_ano_tx_rx_app -V
```

### Test Coverage

Current tests include:

- **`test_demo_ano_tx_rx_app`**: Tests the combined TX+RX application (`demo_ano_tx_rx`) in loopback and NIC mode
  - Verifies the application starts successfully
  - Confirms packets are transmitted and received
  - Validates output contains expected "Received packet" messages

### Test Configuration Files

Test-specific YAML files are located in `tests/`:
- `demo_ano_tx_rx_loopback_test.yaml`: Configuration for combined TX+RX loopback testing
- `demo_ano_tx_test.yaml`: Configuration for TX-only testing (future use)
- `demo_ano_rx_test.yaml`: Configuration for RX-only testing (future use)

> **Note**: Tests for separate TX/RX processes are currently disabled due to DPDK container isolation requirements. The combined TX+RX test provides sufficient coverage for the core functionality.

## Expected Output

### Combined TX+RX Mode (tx_rx or tx_rx_loopback)
```
[info] TensorGeneratorOp initialized on GPU 0
[info] TensorNetworkTxOp initialized: port_id=0, dst=192.168.10.11:5001, GPU-only mode=1
[info] TensorNetworkRxOp initialized: port_id=1, GPU-only mode=1
[info] TensorPrinterOp initialized with 256 byte print buffer
[info] demo_ano_tx_rx configured: GPU -> Network (GPUDirect) -> GPU
[debug] Generated tensor with message: 'hello world #0' (1024 bytes on GPU)
[debug] 📤 Sent 1 packets (1024 bytes)
[info] 📥 Received packet #1: 'hello world #0' (1024 bytes from GPU)
[debug] Generated tensor with message: 'hello world #1' (1024 bytes on GPU)
[debug] 📤 Sent 1 packets (1024 bytes)
[info] 📥 Received packet #2: 'hello world #1' (1024 bytes from GPU)
...
```

### Separate TX Mode
```
[info] TensorGeneratorOp initialized on GPU 0
[info] TensorNetworkTxOp initialized: port_id=0, dst=192.168.10.11:5001, GPU-only mode=1
[info] DemoANOTxApp configured: GPU -> Network (GPUDirect)
[debug] Generated tensor with message: 'hello world #0' (1024 bytes on GPU)
[debug] 📤 Sent 1 packets (1024 bytes)
[debug] Generated tensor with message: 'hello world #1' (1024 bytes on GPU)
[debug] 📤 Sent 1 packets (1024 bytes)
...
```

### Separate RX Mode
```
[info] TensorNetworkRxOp initialized: port_id=0, GPU-only mode=1
[info] TensorPrinterOp initialized with 256 byte print buffer
[info] DemoANORxApp configured: Network (GPUDirect) -> Printer
[info] 📥 Received packet #1: 'hello world #0' (1024 bytes from GPU)
[info] 📥 Received packet #2: 'hello world #1' (1024 bytes from GPU)
...
```

## Troubleshooting

### Build Issues
- Ensure Advanced Network operator dependencies are met
- Verify CUDA toolkit is installed: `nvcc --version`
- Check DPDK packages: `dpkg -l | grep dpdk`
- **Wrong CONNEXTDDS_ARCH**: Verify your platform architecture and adjust the build command accordingly

### Runtime Issues
- **DPDK already in use** (separate TX/RX modes): Each TX and RX process must run in separate containers. Do not use `--local` flag for separate modes.
- **Permission denied / DPDK initialization failed**: 
  - Ensure you're using the required `--docker-opts` with capabilities (`SYS_ADMIN`, `IPC_LOCK`, `NET_ADMIN`)
  - Check hugepages are configured: `cat /proc/meminfo | grep Huge`
  - Verify NIC is bound to DPDK driver: `dpdk-devbind.py --status`
- **No burst available**: Increase `num_bufs` in YAML or decrease `batch_size`
- **Invalid interface**: Check PCIe address with `lspci`
- **No packets received**: 
  - Verify flow rules match UDP ports (5000→5001)
  - Check `flow_isolation: true` is set on RX
  - Ensure destination MAC is correct
- **GPUDirect errors**: 
  - Verify GPU topology: `nvidia-smi topo -m`
  - Check GPU is visible: `nvidia-smi`
  - Ensure NIC supports GPUDirect RDMA

### Network Verification
```bash
# Test connectivity
ping <remote_ip>

# Check NIC link status
ethtool <interface_name>

# Verify MAC address
ip link show <interface_name>
 
# Check DPDK-compatible NICs
lspci | grep -i mellanox
```
