# DPDK-Enabled Dev Container for Connext ANO Basic App

This dev container configuration provides the necessary Linux capabilities for running DPDK applications with the Advanced Network Operator.

## Prerequisites

1. **Docker** installed on your host machine
2. **VS Code** with the **Dev Containers** extension
3. **Hugepages** configured on your host system

### Configure Hugepages on Host

DPDK requires hugepages for memory management. Run on your host:

```bash
# Check current hugepages
cat /proc/meminfo | grep Huge

# Configure 2MB hugepages (1024 pages = 2GB)
sudo sh -c 'echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'

# Make it persistent across reboots
echo "vm.nr_hugepages=1024" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p

# Mount hugepages (if not already mounted)
sudo mkdir -p /dev/hugepages
sudo mount -t hugetlbfs nodev /dev/hugepages
```

## How to Use

### Option 1: Open in VS Code Dev Container

1. Open VS Code
2. Navigate to: `applications/connext/connext_ano_basic_app`
3. Press `F1` or `Ctrl+Shift+P`
4. Type: **"Dev Containers: Reopen in Container"**
5. Select this folder's container configuration

### Option 2: Build and Run Docker Manually

```bash
# Navigate to the application directory
cd /workspace/holohub/applications/connext/connext_ano_basic_app

# Build the Docker image
docker build -t connext-ano-app .

# Run with required capabilities
docker run -it --rm \
  --cap-add=SYS_NICE \
  --cap-add=SYS_ADMIN \
  --cap-add=IPC_LOCK \
  --cap-add=NET_ADMIN \
  --device=/dev/hugepages:/dev/hugepages \
  --ulimit memlock=-1:-1 \
  --privileged \
  -v /dev/hugepages:/dev/hugepages \
  -v $(pwd):/workspace \
  connext-ano-app
```

### Option 3: Docker Compose (Recommended)

Create a `docker-compose.yml` file:

```yaml
version: '3.8'

services:
  connext-ano-app:
    build: .
    container_name: connext-ano-dpdk
    cap_add:
      - SYS_NICE
      - SYS_ADMIN
      - IPC_LOCK
      - NET_ADMIN
    devices:
      - /dev/hugepages:/dev/hugepages
    volumes:
      - /dev/hugepages:/dev/hugepages
      - .:/workspace
    ulimits:
      memlock: -1
    privileged: true
    working_dir: /workspace
    stdin_open: true
    tty: true
```

Then run:
```bash
docker-compose up -d
docker-compose exec connext-ano-app bash
```

## Building and Running Your Application

Once inside the container:

```bash
# Build the application
cd /workspace/holohub
./holohub build connext_ano_basic_app --build-type debug --local

# Run RX application
cd build/connext_ano_basic_app
./connext_ano_basic_app rx --config demo_ano_rx.yaml

# In another terminal, run TX application
./connext_ano_basic_app tx --config demo_ano_tx.yaml
```

## Capabilities Explained

- **SYS_NICE**: Required for setting CPU affinity and thread priorities
- **SYS_ADMIN**: Needed for memory policy operations (`set_mempolicy`)
- **IPC_LOCK**: Allows locking memory (prevents swapping)
- **NET_ADMIN**: Network interface configuration
- **--privileged**: Full access to DPDK devices and network interfaces

## Troubleshooting

### Error: "set_mempolicy: Operation not permitted"
- Ensure the container has `SYS_ADMIN` capability
- Check if running with `--privileged` flag

### Error: "Cannot allocate memory"
- Verify hugepages are configured on host: `cat /proc/meminfo | grep Huge`
- Increase hugepages if needed: `sudo sh -c 'echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'`

### Error: "EAL: Cannot open /dev/hugepages"
- Ensure hugepages are mounted on host
- Check device is passed to container: `--device=/dev/hugepages:/dev/hugepages`

### Network Interface Not Found
- Use `--network=host` if you need direct access to host network interfaces
- Ensure DPDK-compatible NIC is available: `lspci | grep -i ethernet`

## Security Note

Running with `--privileged` and multiple capabilities provides extensive access. For production:
- Use minimal required capabilities only
- Consider using non-privileged containers with specific device access
- Implement proper security policies and network isolation
