# Connext Advanced Network Basic Demo

Demo application showing GPU-to-GPU communication using Holoscan's Advanced Network library with DPDK and GPUDirect.

## Overview

This application demonstrates:
- **DemoANOTxApp**: Generates tensors with "hello world #N" on GPU and transmits via network
- **DemoANORxApp**: Receives tensors from network on GPU and prints contents

Both apps use **GPU-only mode** (no header-data split) for maximum throughput with GPUDirect RDMA.

## Architecture

### TX Pipeline
```
TensorGeneratorOp -> TensorNetworkTxOp -> Network (DPDK + GPUDirect)
```

### RX Pipeline
```
Network (DPDK + GPUDirect) -> TensorNetworkRxOp -> TensorPrinterOp
```

## Prerequisites

1. **DPDK-compatible NIC** (e.g., Mellanox ConnectX-5/6/7)
2. **GPUDirect RDMA** enabled
3. **System configuration** from [High Performance Networking tutorial](/tutorials/high_performance_networking/README.md)
4. **Two machines** connected via high-speed network

## Configuration

Edit the YAML files with your network details:

### `demo_ano_tx.yaml`
Replace the following placeholders:
- `<CHANGE_ME_TX_PCI_ADDRESS>`: PCIe address of TX NIC (e.g., `0005:03:00.0`)
- `<CHANGE_ME_TX_IP>`: TX machine IP address (e.g., `192.168.10.10`)
- `<CHANGE_ME_RX_IP>`: RX machine IP address (e.g., `192.168.10.11`)
- `<CHANGE_ME_RX_MAC>`: RX machine MAC address (e.g., `3c:6d:66:11:91:56`)

### `demo_ano_rx.yaml`
Replace the following placeholder:
- `<CHANGE_ME_RX_PCI_ADDRESS>`: PCIe address of RX NIC (e.g., `0005:03:00.1`)

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

Build the application from the Holohub root:

```bash
./holohub build connext_ano_basic_app --build-type debug
```

You can use the `--local` flag to keep all build artifacts under the host machine:
```bash
./holohub build connext_ano_basic_app --build-type debug --local
```

## Run

Start the receiver first, then the transmitter. Both processes must be configured with matching network parameters.

The build copies configuration files to the build directory:
- Without `--local`: `/workspace/holohub/build/connext_ano_basic_app/applications/connext/connext_ano_basic_app/`
- With `--local`: `applications/connext/connext_ano_basic_app/build/`

### On RX Machine (Receiver)

```bash
./holohub run connext_ano_basic_app \
  --run-args="/workspace/holohub/build/connext_ano_basic_app/applications/connext/connext_ano_basic_app/demo_ano_rx.yaml rx" \
  --docker-opts="-u root --privileged --network=host --cap-add=IPC_LOCK"
```

If using `--local`:
```bash
./holohub run connext_ano_basic_app \
  --run-args="applications/connext/connext_ano_basic_app/demo_ano_rx.yaml rx" \
  --local \
  --docker-opts="-u root --privileged --network=host --cap-add=IPC_LOCK"
```

### On TX Machine (Transmitter)

```bash
./holohub run connext_ano_basic_app \
  --run-args="/workspace/holohub/build/connext_ano_basic_app/applications/connext/connext_ano_basic_app/demo_ano_tx.yaml tx" \
  --docker-opts="-u root --privileged --network=host --cap-add=IPC_LOCK"
```

If using `--local`:
```bash
./holohub run connext_ano_basic_app \
  --run-args="applications/connext/connext_ano_basic_app/demo_ano_tx.yaml tx" \
  --local \
  --docker-opts="-u root --privileged --network=host --cap-add=IPC_LOCK"
```

## Expected Output

**TX:**
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

**RX:**
```
[info] TensorNetworkRxOp initialized: port_id=0, GPU-only mode=1
[info] TensorPrinterOp initialized with 256 byte print buffer
[info] DemoANORxApp configured: Network (GPUDirect) -> Printer
[info] 📥 Received packet #1: 'hello world #0' (1024 bytes from GPU)
📥 Received packet #1: 'hello world #0' (1024 bytes from GPU)
[info] 📥 Received packet #2: 'hello world #1' (1024 bytes from GPU)
📥 Received packet #2: 'hello world #1' (1024 bytes from GPU)
...
```

## Troubleshooting

### Build Issues
- Ensure Advanced Network operator dependencies are met
- Verify CUDA toolkit is installed: `nvcc --version`
- Check DPDK packages: `dpkg -l | grep dpdk`

### Runtime Issues
- **No burst available**: Increase `num_bufs` in YAML or decrease `batch_size`
- **Invalid interface**: Check PCIe address with `lspci`
- **No packets received**: 
  - Verify flow rules match UDP ports (5000→5001)
  - Check `flow_isolation: true` is set on RX
  - Ensure destination MAC is correct
- **DPDK initialization failed**: 
  - Check hugepages: `cat /proc/meminfo | grep Huge`
  - Verify NIC is bound to DPDK driver: `dpdk-devbind.py --status`
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
