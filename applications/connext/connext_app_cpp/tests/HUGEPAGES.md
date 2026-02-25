# Hugepages Requirements for ANO Multi-Container Tests

## Problem

When running multi-container ANO tests (like `test_ano_1_to_many_e2e.py`), the test may fail with:

```
EAL: No free 1048576 kB hugepages reported on node 0
EAL: FATAL: Cannot get hugepage information.
[critical] [adv_network_dpdk_mgr.cpp:457] Invalid EAL arguments: 13
[critical] [adv_network_dpdk_mgr.cpp:174] Failed to initialize DPDK
```

**Critical:** This error typically occurs on the **last container** launched (usually TX), because all hugepages were consumed by previously launched containers.

## Root Cause

Each DPDK-enabled ANO container requires **1 hugepage of 1GB** for memory allocation. Containers are launched sequentially:

1. RX containers launch first (each consuming 1 hugepage)
2. TX containers launch after RX initialization (each consuming 1 hugepage)

**Container requirements:**
- `test_ano_e2e.py`: **2 containers** (1 TX + 1 RX) = **2 hugepages needed** ✅
- `test_ano_1_to_many_e2e.py`: **4 containers** (1 TX + 3 RX) = **4 hugepages needed** ❌
- `test_ano_many_to_many_e2e.py`: **6 containers** (2 TX + 4 RX) = **6 hugepages needed** ❌

**Example failure scenario with 3 total hugepages:**
- Start: 3 total, 2 free
- Launch RX1: 3 total, 1 free ✓
- Launch RX2: 3 total, 0 free ✓
- Launch RX3: **Blocks/consumes last hugepage** ⚠️
- Launch TX: **FAILS** ❌ - No hugepages available

## Protection Mechanism

Starting with the refactored code, tests **validate hugepage availability before launching each container**:

```python
# Before launching each container
if not check_hugepage_availability(1, "RX Subscriber 1"):
    raise RuntimeError("Insufficient hugepages")
```

This prevents launching containers that will fail, and provides clear error messages:

```
✗ Insufficient hugepages for RX Subscriber 3: 0 available, 1 required
✗ Each ANO container requires 1 hugepage (1GB)

To allocate more hugepages, run as root:
  echo 4 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
```

## Checking Current Hugepages

```bash
cat /proc/meminfo | grep -i huge
```

Example output:
```
HugePages_Total:       3
HugePages_Free:        0
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:    1048576 kB
Hugetlb:         3145728 kB
```

In this example:
- **Total:** 3 hugepages total
- **Free:** 0 hugepages available
- **Not sufficient for 1-to-many test** (needs 4)

## Solution: Allocate More Hugepages

### Temporary Allocation (until reboot)

```bash
# Allocate 6 hugepages (sufficient for many-to-many test)
sudo sh -c 'echo 6 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages'

# Verify
cat /proc/meminfo | grep HugePages_Total
# Should show: HugePages_Total:       6
```

### Permanent Allocation (persists across reboots)

Add to `/etc/sysctl.conf`:

```bash
# Allocate 6 hugepages of 1GB each
vm.nr_hugepages = 6
```

Then apply:

```bash
sudo sysctl -p
```

### Recommended Allocations

| Test Type | Containers | Hugepages Required | Recommended |
|-----------|------------|-------------------|-------------|
| 1-to-1 (test_ano_e2e) | 2 | 2 | 3 |
| 1-to-many (default 3 RX) | 4 | 4 | 5 |
| 1-to-many (max 10 RX) | 11 | 11 | 12 |
| many-to-many (2 TX, 4 RX) | 6 | 6 | 7 |

**General rule:** Allocate `N + 1` hugepages where `N` is the total number of containers.

## Test Validation

Starting with this commit, the ANO tests include automatic hugepage validation:

```python
# In test_ano_1_to_many_e2e.py
validator.validate_hugepages(num_containers=4)
```

If insufficient hugepages are detected, the test will fail early with a clear error message:

```
✗ Insufficient hugepages: 2 available, 4 required
✗ Each ANO container requires 1 hugepage (1GB)

To allocate more hugepages, run as root:
```

### Race Condition Protection

The multi-container orchestrator also protects against a timing race condition:

**Problem:** Docker takes ~1-2 seconds to actually reserve hugepages after launching a container. If we launch containers too quickly, the hugepage validation passes (reading stale "free" count) but Docker later fails to reserve, causing `Exit code: 137`.

**Solution:** The orchestrator now waits 2 seconds after launching each container before validating/launching the next one:

```python
# In MultiProcessOrchestrator._start_rx_containers()
process = subprocess.Popen([script, str(sub_id)], ...)
processes.append(process)

# Wait for Docker to reserve hugepages before next container
time.sleep(2)
```

This ensures:
1. Container launches
2. Docker reserves hugepage
3. System updates free hugepage count
4. Next validation reads correct count

Without this delay, you may see all containers starting but dying with `Exit code: 137` even when you have sufficient hugepages allocated.
  echo 4 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
```

## Troubleshooting

### Cannot allocate hugepages

If allocation fails:

```bash
sudo sh -c 'echo 6 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages'
cat /proc/meminfo | grep HugePages_Total
# Still shows old value
```

**Possible causes:**
1. **Insufficient free memory**: Free up RAM before allocating hugepages
2. **Memory fragmentation**: Allocate hugepages early after boot
3. **NUMA constraints**: Check NUMA configuration with `numactl --hardware`

**Solution:**
```bash
# Free cache memory
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'

# Try allocation again
sudo sh -c 'echo 6 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages'
```

### Hugepages allocated but containers still fail

If hugepages show as allocated but containers fail:

1. **Check if hugepages are mounted:**
   ```bash
   mount | grep hugepages
   ```

2. **Verify /dev/hugepages is accessible:**
   ```bash
   ls -la /dev/hugepages
   ```

3. **Check for stale DPDK processes:**
   ```bash
   # Stop all docker containers
   docker stop $(docker ps -aq)
   
   # Verify hugepages freed
   cat /proc/meminfo | grep HugePages_Free
   ```

## Performance Impact

Hugepages improve performance by:
- Reducing TLB misses (fewer page table lookups)
- Enabling zero-copy DPDK packet processing
- Supporting GPU-Direct RDMA transfers

**Memory overhead:** Each 1GB hugepage is locked in RAM and cannot be swapped.

## References

- [DPDK Hugepages Documentation](https://doc.dpdk.org/guides/linux_gsg/sys_reqs.html#use-of-hugepages-in-the-linux-environment)
- [Holoscan ANO Configuration Guide](../../README.md)
- [Linux Hugepages Documentation](https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt)
