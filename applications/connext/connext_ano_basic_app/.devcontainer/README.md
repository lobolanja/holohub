# DPDK-Enabled Dev Container for Connext ANO Basic App

This dev container configuration provides the necessary Linux capabilities for running DPDK applications with the Advanced Network Operator (ANO).

Please follow the [High Performance Networking tutorial](../../../tutorials/high_performance_networking) to properly configure your environment.

## Prerequisites

You must have a **NVIDIA ConnectX SmartNIC** (Mellanox) and a **discrete GPU** for high-performance communication. See the [High Performance Networking tutorial](../../../tutorials/high_performance_networking) for detailed requirements.

1. **Docker** installed on your host machine
2. **VS Code** with the **Dev Containers** extension
3. **Hugepages** configured on your host system (minimum 2048 x 2MB pages recommended)
4. **RTI Connext DDS license file** (`rti_license.dat`)
5. **NVIDIA ConnectX NIC** with Mellanox OFED drivers installed on host

## Quick Start Checklist

Before running the application, verify:

- [ ] Hugepages are configured: `cat /proc/meminfo | grep Huge`
- [ ] NVIDIA ConnectX NIC is available: `lspci | grep -i mellanox`
- [ ] RTI license file is available at the specified path
- [ ] Docker has necessary permissions (run with `sudo`)

## How to Use

### Build and Run Docker using holohub command

Ensure your RTI license file is located on the host machine. The command below mounts it as a volume so the container can access it.

```bash
sudo ./holohub run-container connext_ano_basic_app \
  --docker-opts="-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat \
  --cap-add=SYS_ADMIN \
  --cap-add=IPC_LOCK \
  --cap-add=NET_ADMIN \
  --device=/dev/hugepages:/dev/hugepages \
  --ulimit memlock=-1:-1 \
  --privileged \
  -v /dev/hugepages:/dev/hugepages"
```

**Note:** You can attach to the running container at any time using VS Code's **"Dev Containers: Attach to Running Container"** command.

## Verifying Setup

Before building, verify the environment is properly configured inside the container:

```bash
# Verify hugepages
cat /proc/meminfo | grep Huge

# Expected output should show:
# HugePages_Total: 2048 (or your configured amount)
# HugePages_Free: Should have available pages

# Check NVIDIA ConnectX NIC
lspci | grep -i mellanox

# Verify network interfaces
ip link show
```

## Building and Running Your Application

Once inside the container:

### 1. Build the Application

```bash
cd /workspace/holohub

# Determine your RTI Connext architecture
# For ARM64: armv8Linux4gcc7.3.0
# For x86_64: x64Linux4gcc7.3.0
# Check with: uname -m

./holohub build connext_ano_basic_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0"
```

### 2. Run the Receiver (RX) Application

The RX application receives data via DPDK and ANO. Start this first:

```bash
# Option 1: Using holohub command
./holohub run connext_ano_basic_app \
  --run-args='rx applications/connext/connext_ano_basic_app/demo_ano_rx.yaml' --local

# Option 2: Direct execution
cd build/connext_ano_basic_app/applications/connext/connext_ano_basic_app
./connext_ano_basic_app rx demo_ano_rx.yaml
```

**Expected output:** Application should initialize Mellanox DPDK, bind to network interface, and wait for incoming data.

### 3. Run the Transmitter (TX) Application

In a separate terminal, attach to the same container and run the TX application:

```bash
# Option 1: Using holohub command
./holohub run connext_ano_basic_app \
  --run-args='tx applications/connext/connext_ano_basic_app/demo_ano_tx.yaml' --local

# Option 2: Direct execution
cd build/connext_ano_basic_app/applications/connext/connext_ano_basic_app
./connext_ano_basic_app tx demo_ano_tx.yaml
```

**Expected output:** Application should start sending data packets, and you should see corresponding reception logs in the RX terminal.

## Configuration Files

The YAML configuration files control:
- Network interface settings (Mellanox DPDK port configuration)
- DDS domain and QoS policies
- Data rate and packet sizes
- Memory allocation parameters

Edit `demo_ano_rx.yaml` and `demo_ano_tx.yaml` to customize behavior for your network setup.

## Docker Capabilities Explained
## Docker Capabilities Explained

The container requires specific Linux capabilities for Mellanox DPDK operation:

- **SYS_ADMIN**: Required for memory policy operations (`set_mempolicy`) and NUMA node access
- **IPC_LOCK**: Allows locking memory to prevent swapping (critical for real-time performance)
- **NET_ADMIN**: Enables network interface configuration and control
- **--privileged**: Provides full access to Mellanox NIC and network devices
- **--ulimit memlock=-1:-1**: Removes memory locking limits (required for DPDK)
- **--device=/dev/hugepages**: Exposes hugepage device to container
## Troubleshooting

### Error: "set_mempolicy: Operation not permitted"
**Cause:** Container lacks necessary capabilities.

**Solution:**
- Ensure the container has `SYS_ADMIN` capability in docker-opts
- Verify running with `--privileged` flag
- Check with: `docker inspect <container_id> | grep -i cap`

### Error: "Cannot allocate memory" or "EAL: Cannot allocate memory"
**Cause:** Insufficient or unconfigured hugepages.

**Solution:**
```bash
# Check current hugepages
cat /proc/meminfo | grep Huge

# Configure 2GB of hugepages (2048 x 2MB pages)
sudo sh -c 'echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'

# Make persistent across reboots
echo "vm.nr_hugepages=2048" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### Error: "EAL: Cannot open /dev/hugepages" or "No such file or directory"
**Cause:** Hugepages not properly mounted or passed to container.

**Solution:**
- Verify hugepages mount on host: `mount | grep huge`
- Ensure device is passed: `--device=/dev/hugepages:/dev/hugepages`
- Check volume mount: `-v /dev/hugepages:/dev/hugepages`

### Network Interface Not Found or "EAL: No available ports"
**Cause:** NVIDIA ConnectX NIC not visible to container or Mellanox OFED drivers not properly installed.

**Solution:**
```bash
# Check for Mellanox NICs
lspci | grep -i mellanox

# Verify network interfaces
ip link show

# Check for InfiniBand/RDMA devices
ls -l /dev/infiniband/

# Verify Mellanox OFED installation (on host)
ofed_info -s
```

### RTI Connext License Error
**Cause:** License file not found or not properly mounted.

**Solution:**
- Verify license file exists: `ls -l ./rti_license.dat`
- Check mount path matches RTI installation directory
- Verify inside container: `ls -l /opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat`

### Application Crashes or Performance Issues
**Possible causes:**
- NUMA node misconfiguration
## Additional Resources

- [High Performance Networking Tutorial](../../../tutorials/high_performance_networking)
- [RTI Connext DDS Documentation](https://community.rti.com/documentation)
- [NVIDIA Mellanox DPDK Documentation](https://docs.nvidia.com/networking/display/MLNXOFEDv461000/DPDK)
- [GPU Direct RDMA Guide](https://docs.nvidia.com/cuda/gpudirect-rdma/)
- [Advanced Network Operator Guide](../../../operators/advanced_network/README.md)
# Check NUMA topology and device placement
numactl --hardware

# Check GPU and NIC NUMA affinity
nvidia-smi topo -m
### Application Crashes or Performance Issues
**Possible causes:**
- NUMA node misconfiguration
- NIC on wrong NUMA node
- Insufficient CPU cores allocated
- Network interface not in correct state

**Debugging steps:**
```bash
# Check NUMA topology and device placement
numactl --hardware

# Check NIC NUMA affinity
lspci -vv -s <pci_address> | grep NUMA
## Additional Resources

- [High Performance Networking Tutorial](../../../tutorials/high_performance_networking)
- [RTI Connext DDS Documentation](https://community.rti.com/documentation)
- [NVIDIA Mellanox DPDK Documentation](https://docs.nvidia.com/networking/display/MLNXOFEDv461000/DPDK)
- [Advanced Network Operator Guide](../../../operators/advanced_network/README.md)


```

## Additional Resourcesux profiles to restrict container capabilities
- Regularly audit container permissions and update security policies
