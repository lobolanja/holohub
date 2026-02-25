# ANO Multi-Topology End-to-End Test Plan

## Overview

This document outlines the plan to extend the existing ANO (Advanced Network Operator) end-to-end test suite with two new network topology scenarios:

1. **1-to-Many**: One publisher sending to multiple subscribers (each on different UDP ports)
2. **Many-to-Many**: Multiple publishers sending to multiple subscribers, where each subscriber receives from at least two different publishers
3. **Mixed-Transport (ANO + DDS)**: One TX sending to two RX containers simultaneously — RX1 via ANO (GPU Direct / DPDK) and RX2 via DDS — validating that both transports operate correctly in parallel from a single dual-mode TX
4. **Shell Script Refactoring**: Consolidate the 8 near-identical launcher scripts into a 2-file library + dispatcher, eliminating ~350 lines of duplicated boilerplate

## Current State

The existing test suite (`test_ano_e2e.py`) validates basic 1-to-1 communication between a single TX and RX container using:
- Physical NICs with DPDK
- GPU-Direct RDMA
- Rate-based transmission (10 msg/sec)
- 80% success threshold validation

The test infrastructure follows SOLID principles with:
- `TestScriptPaths`: Path management
- `EnvironmentValidator`: Environment validation (ANO vs DDS)
- `OutputParser`: Result parsing with transport-specific fallbacks
- `ContainerOrchestrator`: Container lifecycle management
- `E2ETestRunner`: Unified test execution

## Test Objectives

### 1-to-Many Test
- Validate that one publisher can reliably send to N subscribers simultaneously
- Verify each subscriber independently receives ≥80% of messages
- Ensure no message duplication or cross-contamination between subscribers
- Confirm proper UDP port routing using DPDK flow rules

### Many-to-Many Test
- Validate that M publishers can send to N subscribers concurrently
- Verify each subscriber correctly receives from its configured publishers only
- Ensure proper message attribution (subscriber can distinguish publisher sources)
- Validate overall system throughput and reliability under multi-publisher load

### Mixed-Transport Test (ANO + DDS)
- Validate that a single TX can run both `enable_ano: true` and `enable_dds: true` simultaneously (dual-mode)
- Verify RX1 (ANO path) receives via DPDK GPU Direct on the physical NIC
- Verify RX2 (DDS path) receives via standard DDS on the same domain — no ANO stack required
- Confirm no cross-contamination: RX1 only counts ANO payloads, RX2 only counts DDS payloads
- Validate that `ConnextTxOp` correctly dispatches to both transports in one `compute()` cycle

## Test Architecture

### Port Assignment Strategy

**1-to-Many Topology:**
- TX Publisher: UDP port 5000
- RX Subscriber 1: UDP port 5001
- RX Subscriber 2: UDP port 5002
- RX Subscriber 3: UDP port 5003

**Mixed-Transport Topology (ANO + DDS):**
- TX: `enable_ano: true` + `enable_dds: true` (dual-mode), ANO UDP port 7000
- RX1 (ANO): `enable_ano: true`, `enable_dds: false` — DPDK, UDP port 7000, physical NIC
- RX2 (DDS): `enable_ano: false`, `enable_dds: true` — standard DDS, domain 42, no NIC/hugepages needed
- DDS domain: 42 (matching existing `test_dds_tx.yaml` / `test_dds_rx.yaml` convention)
- DDS topic: `E2ETestTopicMixed` (distinct from the pure-DDS test topic to avoid subscriber cross-talk)

**Many-to-Many Iterative Topologies** (built up incrementally):

*Intermediate A — Many-to-One (2TX→1RX):*
- TX Publisher 1: UDP src port 6001, dst port 6001
- TX Publisher 2: UDP src port 6002, dst port 6001
- RX Subscriber 1: Receives from both publishers (dual DPDK flow rules, same queue)

*Intermediate B — Many-to-Two (2TX→2RX):*
- TX Publisher 1: UDP src port 6001
- TX Publisher 2: UDP src port 6002
- RX Subscriber 1: dst port 6001 — receives from Pub1 (udp_src 6001) and Pub2 (udp_src 6002)
- RX Subscriber 2: dst port 6002 — receives from Pub1 (udp_src 6001) and Pub2 (udp_src 6002)
- Both subscribers have two DPDK flow rules (one per publisher) routing to queue 0

*Target — Full Many-to-Many (2TX→3RX):*
- TX Publisher 1: UDP port 6001 → RX Subscribers 1, 2
- TX Publisher 2: UDP port 6002 → RX Subscribers 1, 3
- RX Subscriber 1: Receives from Publishers 1, 2 (ports 6001, 6002)
- RX Subscriber 2: Receives from Publisher 1 (port 6001)
- RX Subscriber 3: Receives from Publisher 2 (port 6002)

This ensures at least one subscriber (RX1) receives from both publishers.

### Container Orchestration Timing

To avoid initialization race conditions and resource conflicts:

1. **Start all RX containers first**
   - Stagger each RX container startup by 2 seconds
   - Allows DPDK to properly initialize each NIC queue
   - Prevents hugepage allocation conflicts

2. **Wait 5 seconds after all RX containers are running**
   - Ensures all receivers are ready to accept data
   - Allows DPDK flows to be fully configured

3. **Start all TX containers**
   - Stagger each TX container startup by 1 second
   - Begin transmission after initialization (5s discovery_wait_ms)

4. **Run test for configured duration** (90 seconds default)

5. **Stop all containers and collect outputs**

### Resource Management

**Environment Variables (Hardware Configuration):**
- `TEST_TX_NIC_PCIE`: TX NIC PCIe address (required)
- `TEST_RX_NIC_PCIE`: RX NIC PCIe address (required)
- `RTI_LICENSE_FILE`: RTI license file path (optional, default: `./rti_license.dat`)

**Test Parameters (Topology Configuration):**
- `num_subscribers`: Number of subscribers (configurable via command-line, default: 3 for 1-to-many, 4 for many-to-many)
- `num_publishers`: Number of publishers (configurable via command-line, default: 1 for 1-to-many, 2 for many-to-many)
- These are **test-level parameters**, not environment variables
- Can be overridden via command-line arguments: `--num-subscribers N` or `--num-publishers M`

**Resource Validation:**
- Check available hugepages before starting (needs at least 51200 * num_containers)
- Validate sufficient GPU memory for all containers
- Ensure DPDK runtime directories are unique per container

## Implementation Plan

### Step 1: Configuration Files

**1-to-Many Configurations** (`tests/config/`):

Create the following YAML files based on existing `test_ano_tx.yaml` and `test_ano_rx.yaml`:

1. `test_ano_tx_1_to_many.yaml`
   - Single TX broadcasting to all subscribers
   - `ano_fast_port: 5000`
   - No flow changes needed (TX doesn't filter)

2. `test_ano_rx_1_to_many_sub1.yaml`
   - Listens on UDP port 5001
   - Flow match: `udp_dst: 5001`
   - `ano_fast_port: 5001`

3. `test_ano_rx_1_to_many_sub2.yaml`
   - Listens on UDP port 5002
   - Flow match: `udp_dst: 5002`
   - `ano_fast_port: 5002`

4. `test_ano_rx_1_to_many_sub3.yaml`
   - Listens on UDP port 5003
   - Flow match: `udp_dst: 5003`
   - `ano_fast_port: 5003`

**Many-to-Many Configurations** (`tests/config/`):

1. `test_ano_tx_many_to_many_pub1.yaml`
   - TX Publisher 1
   - `ano_fast_port: 6001`
   - Payload prefix: `pub1_payload`

2. `test_ano_tx_many_to_many_pub2.yaml`
   - TX Publisher 2
   - `ano_fast_port: 6002`
   - Payload prefix: `pub2_payload`

3. `test_ano_rx_many_to_many_sub1.yaml`
   - Receives from Publishers 1 and 2
   - Multiple flow entries for ports 6001 and 6002
   - `ano_fast_port: 6001` (primary)

4. `test_ano_rx_many_to_many_sub2.yaml`
   - Receives from Publisher 1 only
   - Flow match: `udp_dst: 6001`
   - `ano_fast_port: 6001`

5. `test_ano_rx_many_to_many_sub3.yaml`
   - Receives from Publisher 2 only
   - Flow match: `udp_dst: 6002`
   - `ano_fast_port: 6002`

**Configuration Parameters to Modify:**
- `connext_tx.ano_fast_port`: Transmitter UDP port
- `connext_rx.ano_fast_port`: Receiver UDP port
- `advanced_network.interfaces.rx.flows.match.udp_dst`: Flow rule destination port
- `payload_source.base_payload`: Unique prefix per publisher (many-to-many)

### Step 1.5: Update CMakeLists.txt

**File:** `cpp/CMakeLists.txt`

**Updates Required:**

Add all new 1-to-many test files to the `connext_app_tests` custom target so they are copied to the build directory during compilation:

1. **Python Test File:**
   - `test_ano_1_to_many_e2e.py`

2. **Shell Scripts:**
   - `run_e2e_tx_1_to_many_container.sh`
   - `run_e2e_rx_1_to_many_container.sh`

3. **YAML Configuration Files:**
   - `test_ano_tx_1_to_many.yaml`
   - `test_ano_rx_1_to_many_sub1.yaml`
   - `test_ano_rx_1_to_many_sub2.yaml`
   - `test_ano_rx_1_to_many_sub3.yaml`

**Implementation:**
- Add `copy_if_different` commands for each new file
- Add all new files to the `DEPENDS` section to track changes
- Ensure files are copied to correct subdirectories (tests/, tests/scripts/, tests/config/)

**Validation:**
- After running CMake, verify all files exist in the build directory
- Check that file modifications trigger rebuild of the `connext_app_tests` target

### Step 2: Shell Scripts

**1-to-Many Scripts** (`tests/scripts/`):

1. `run_e2e_tx_1_to_many_container.sh`
   - Parameterless (single TX)
   - Uses `config/test_ano_tx_1_to_many.yaml`
   - Similar to existing `run_e2e_tx_container.sh`

2. `run_e2e_rx_1_to_many_container.sh`
   - Takes subscriber ID as parameter: `$1` (1, 2, or 3)
   - Uses `config/test_ano_rx_1_to_many_sub${1}.yaml`
   - Creates unique DPDK runtime dir: `/home/$USER/dpdk_rx_sub${1}`

**Many-to-Many Scripts** (`tests/scripts/`):

1. `run_e2e_tx_many_to_many_container.sh`
   - Takes publisher ID as parameter: `$1` (1 or 2)
   - Uses `config/test_ano_tx_many_to_many_pub${1}.yaml`
   - Creates unique DPDK runtime dir: `/home/$USER/dpdk_tx_pub${1}`

2. `run_e2e_rx_many_to_many_container.sh`
   - Takes subscriber ID as parameter: `$1` (1, 2, or 3)
   - Uses `config/test_ano_rx_many_to_many_sub${1}.yaml`
   - Creates unique DPDK runtime dir: `/home/$USER/dpdk_rx_sub${1}`

**Script Requirements:**
- Validate subscriber/publisher ID parameter
- Check config file exists before launching
- Pass container-specific environment variables
- Use unique Docker container names to avoid conflicts

### Step 3: Extended Test Utilities

Extend `test_utils.py` with new classes following SOLID principles:

**New Classes:**

1. **`TopologyTestParameters`** (dataclass)
   - `num_publishers: int` (default values set per test)
   - `num_subscribers: int` (default values set per test)
   - `port_mapping: Dict[str, List[int]]` (maps publisher → subscriber ports)
   - `timeout_seconds: int = 90`
   - `success_threshold: float = 0.80`
   - `rx_init_wait: int = 5`
   - `rx_stagger_seconds: int = 2`
   - `tx_stagger_seconds: int = 1`
   - Factory methods: `for_1_to_many(num_subscribers=3)`, `for_many_to_many(num_publishers=2, num_subscribers=4)`

2. **`MultiProcessOrchestrator`**
   - Manages N TX and M RX processes concurrently
   - Methods:
     - `run_containers() -> Tuple[List[str], List[str]]`: Returns (tx_outputs, rx_outputs)
     - `_start_rx_containers() -> List[subprocess.Popen]`
     - `_start_tx_containers() -> List[subprocess.Popen]`
     - `_stop_all_containers(processes: List[subprocess.Popen])`
     - `_collect_outputs(processes: List[subprocess.Popen]) -> List[str]`
   - Follows Single Responsibility: only handles container lifecycle

3. **`MultiTopologyOutputParser`**
   - Aggregates parsing results from multiple processes
   - Methods:
     - `parse_multiple(tx_outputs: List[str], rx_outputs: List[str], topology: TopologyTestParameters) -> MultiTopologyResults`
     - `_validate_subscriber_isolation(results: MultiTopologyResults) -> bool`: Checks no message duplication
     - `_validate_publisher_attribution(results: MultiTopologyResults) -> bool`: Verifies correct publisher→subscriber mapping
   - Returns: `MultiTopologyResults` dataclass with per-process and aggregate metrics

4. **`MultiTopologyResults`** (dataclass)
   - `tx_results: List[TestResults]` (one per publisher)
   - `rx_results: List[TestResults]` (one per subscriber)
   - `total_sent: int`
   - `total_received: int`
   - `per_subscriber_received: Dict[int, int]`
   - `per_publisher_sent: Dict[int, int]`
   - Methods:
     - `overall_success_rate() -> float`
     - `meets_threshold(threshold: float) -> bool`

**Design Principles Applied:**
- **Single Responsibility**: Each class has one reason to change
- **Open/Closed**: Extend existing classes, don't modify them
- **Dependency Inversion**: Depend on abstractions (base classes), not concrete implementations
- **Interface Segregation**: Small, focused interfaces

### Step 4: Test Implementation - 1-to-Many

**File:** `test_ano_1_to_many_e2e.py`

**Structure:**

```python
#!/usr/bin/env python3
"""
ANO 1-to-Many End-to-End Test

Tests one publisher sending to N subscribers (N=3 by default).
Each subscriber listens on a different UDP port.

Prerequisites:
    - Physical NICs with TEST_TX_NIC_PCIE and TEST_RX_NIC_PCIE environment variables
    - RTI license file (RTI_LICENSE_FILE or ./rti_license.dat)
    - Same hardware requirements as test_ano_e2e.py

Usage:
    # Basic usage with defaults (3 subscribers)
    export TEST_TX_NIC_PCIE="0005:03:00.0"
    export TEST_RX_NIC_PCIE="0005:03:00.1"
    python3 test_ano_1_to_many_e2e.py
    
    # Custom number of subscribers
    python3 test_ano_1_to_many_e2e.py --num-subscribers 2
    
    # With custom timeout
    python3 test_ano_1_to_many_e2e.py --num-subscribers 4 --timeout 120
"""
```

**Test Flow:**
1. Parse command-line arguments (num_subscribers, timeout, etc.)
2. Validate environment (NICs, license)
3. Validate topology parameters (num_subscribers ≤ max supported)
4. Start N RX containers (staggered by 2s)
5. Wait 5s for initialization
6. Start 1 TX container
7. Run for configured timeout (default 90s)
8. Stop all containers
9. Parse outputs (1 TX output, N RX outputs)
10. Validate:
    - TX sent > 0 messages
    - Each RX received ≥ 80% of TX messages
    - No duplicate messages across subscribers
    - Total received ≤ N * TX sent (no duplication)

**Exit Codes:**
- 0: All subscribers met threshold
- 1: Environment validation failed
- 2: One or more subscribers below threshold
- 3: Message duplication detected

### Step 5: Test Implementation - Many-to-Many

**File:** `test_ano_many_to_many_e2e.py`

**Structure:**

```python
#!/usr/bin/env python3
"""
ANO Many-to-Many End-to-End Test

Tests M publishers sending to N subscribers where each subscriber
receives from at least 2 publishers.

Default topology (2 publishers, 3 subscribers):
- Publisher 1 → Subscribers 1, 2
- Publisher 2 → Subscribers 1, 3

Prerequisites:
    - Physical NICs with TEST_TX_NIC_PCIE and TEST_RX_NIC_PCIE environment variables
    - RTI license file (RTI_LICENSE_FILE or ./rti_license.dat)
    - Same hardware requirements as test_ano_e2e.py

Usage:
    # Basic usage with defaults (2 publishers, 3 subscribers)
    export TEST_TX_NIC_PCIE="0005:03:00.0"
    export TEST_RX_NIC_PCIE="0005:03:00.1"
    python3 test_ano_many_to_many_e2e.py
    
    # Custom topology (requires corresponding config files)
    python3 test_ano_many_to_many_e2e.py --num-publishers 3 --num-subscribers 5
    
    # With custom timeout and threshold
    python3 test_ano_many_to_many_e2e.py --timeout 120 --threshold 0.75
"""
```

**Test Flow:**
1. Parse command-line arguments (num_publishers, num_subscribers, timeout, etc.)
2. Validate environment (NICs, license)
3. Validate topology parameters and build port mapping
4. Start N RX containers (staggered by 2s)
5. Wait 5s for initialization
6. Start M TX containers (staggered by 1s)
7. Run for configured timeout (default 90s)
8. Stop all containers
9. Parse outputs (M TX outputs, N RX outputs)
10. Validate:
    - Each publisher sent > 0 messages
    - Each subscriber received ≥ configured threshold from its publishers
    - Publisher attribution is correct (by payload prefix matching)
    - Subscribers only received from their configured publishers
    - No message loss within threshold

**Exit Codes:**
- 0: All validations passed
- 1: Environment validation failed
- 2: One or more subscribers below threshold
- 3: Publisher attribution incorrect
- 4: Subscriber received from wrong publisher

### Step 6: Documentation Updates ✅ COMPLETE (Feb 25, 2026)

**Files Updated:**

1. **`tests/README.md`** ✅
   - Added section "Multi-Topology ANO Tests" with full ASCII topology diagrams
   - Documented 1-to-many test usage and config table
   - Documented many-to-many test usage, subscription mapping, and config table
   - Updated "Test Structure" file tree with all new scripts and configs
   - Added Pre-flight Checklist section
   - Added Multi-Topology Troubleshooting section (hugepages + SMMU fix)
   - Updated Table of Contents with all new section anchors

2. **Topology diagrams** (ASCII art in README) ✅:
   ```
   1-to-Many:
   
   TX (port 5000) ──┬──> RX1 (port 5001)
                    ├──> RX2 (port 5002)
                    └──> RX3 (port 5003)
   
   Many-to-Many:
   
   Pub1 (port 6001) ──> Sub1 (dual-flow), Sub2
   Pub2 (port 6002) ──> Sub1 (dual-flow), Sub3
   ```

3. **Test execution instructions** ✅
   - Pre-flight checklist (hugepages, stale container cleanup, DPDK runtime cleanup)
   - Expected output examples already documented in Step 5 results blocks
   - Troubleshooting: hugepages, SMMU exhaustion (`num_bufs` fix), Exit 137 race

### Step 7: Mixed-Transport End-to-End Test (ANO + DDS)

**Goal:** Validate that a single TX can broadcast to two receivers simultaneously — one over ANO (GPU Direct) and one over DDS — confirming the `enable_ano: true` + `enable_dds: true` dual-mode path of `ConnextTxOp`.

**Step 7.0: Enable Dual-Transport in `ConnextTxOp`** ✅ COMPLETE (Feb 25, 2026)

Full implementation details documented in `applications/connext/TASK.md` (Task 1). Summary of C++ changes:

1. **Removed mutual-exclusion guard** from `ConnextTxOp::start()` — replaced `if/else if` single-transport init with independent `if` blocks so both `dds_writer_` and `ano_writer_` are constructed simultaneously.
2. **Updated `compute()`** — ANO path runs first (GPU pointer, zero-copy RDMA); DDS path runs second with an internal `cudaMemcpy` GPU→CPU copy; both fire per tick when both transports are enabled.
3. **Added `use_ano` field** to `DemoAppConfig` and fixed `use_gpu` derivation in `configure_tx_operators()` so GPU memory is allocated whenever ANO is active (previously gated on `!use_dds`, which was wrong in dual-transport mode).
4. **Fixed `adv_net_init()` gate** in `compose()` from `!use_dds` to `use_ano` — the ANO manager was never initialised in dual-transport mode, causing an assertion failure.
5. **Fixed false-positive transport isolation check** in `test_mixed_transport_e2e.py` — tightened `_DDS_INDICATORS` and `_ANO_INDICATORS` to runtime-only keywords (removed broad init-log terms like `'DDS'`, `'connext'`, `'hugepage'` that appear in every container regardless of transport).

**Topology:**

```
┌─────────────────────────────────────────────────┐
│  TX Container (dual-mode)                        │
│  enable_ano: true  │  enable_dds: true            │
│  ANO port: 7000    │  DDS domain: 42              │
└──────────┬──────────────────────┬────────────────┘
           │ DPDK/GPUDirect        │ DDS (UDP/mcast)
           ▼                       ▼
┌──────────────────┐   ┌───────────────────────────┐
│  RX1 (ANO)       │   │  RX2 (DDS)                │
│  enable_ano: true │   │  enable_dds: true          │
│  enable_dds: false│   │  enable_ano: false         │
│  UDP port: 7000   │   │  domain: 42               │
│  Physical NIC     │   │  No NIC / no hugepages    │
└──────────────────┘   └───────────────────────────┘
```

**Step 7.1: Configuration Files**

Create three new YAML files in `tests/config/`:

1. `test_mixed_tx.yaml` — dual-mode TX
   - Based on `test_ano_tx.yaml` but with **both** transports enabled:
     ```yaml
     connext_tx:
       enable_dds: true          # DDS data plane active
       enable_ano: true          # ANO GPU Direct active
       domain_id: 42
       topic_name: E2ETestTopicMixed
       ano_channel: E2ETestChannelMixed
       ano_buffer_id: e2e_mixed_buffer_tx
       ano_fast_port: 7000
       # all other ANO fields identical to test_ano_tx.yaml
     payload_source:
       base_payload: mixed_test_payload
       use_gpu_memory: true      # GPU memory works for both paths
     ```
   - Requires physical TX NIC (`TEST_TX_NIC_PCIE`) for ANO path
   - `num_bufs: 16384` (SMMU-safe, see Lesson 6)

2. `test_mixed_rx_ano.yaml` — ANO-only RX (RX1)
   - Based on `test_ano_rx.yaml`:
     ```yaml
     connext_rx:
       enable_dds: false
       enable_ano: true
       ano_channel: E2ETestChannelMixed   # MUST match TX
       ano_buffer_id: e2e_mixed_buffer_rx1
       ano_fast_port: 7000
     advanced_network:
       cfg:
         interfaces:
         - rx:
             flows:
             - match:
                 udp_src: 7000
                 udp_dst: 7000
     payload_sink:
       expected_payload_prefix: mixed_test_payload
     ```
   - Requires physical RX NIC (`TEST_RX_NIC_PCIE`)
   - `num_bufs: 16384`

3. `test_mixed_rx_dds.yaml` — DDS-only RX (RX2)
   - Based on `test_dds_rx.yaml`:
     ```yaml
     connext_rx:
       enable_dds: true
       enable_ano: false
       domain_id: 42
       topic_name: E2ETestTopicMixed      # MUST match TX
     payload_sink:
       expected_payload_prefix: mixed_test_payload   # same prefix, different parser
     demo:
       discovery_wait_ms: 0              # DDS handles discovery
     ```
   - **No** `advanced_network` block (no DPDK, no hugepages)
   - Can run with `--network=host` like the DDS test

**Step 7.2: Shell Scripts**

Create in `tests/scripts/`:

1. `run_e2e_tx_mixed_container.sh` — launches dual-mode TX
   - Needs both `TEST_TX_NIC_PCIE` (for ANO) and standard network access (for DDS)
   - Sets `--network=host` AND passes PCIe device
   - Similar to `run_e2e_tx_container.sh`; switch config file to `test_mixed_tx.yaml`

2. `run_e2e_rx_mixed_ano_container.sh` — launches ANO RX (RX1)
   - Identical pattern to `run_e2e_rx_container.sh`; switch config to `test_mixed_rx_ano.yaml`
   - Needs `TEST_RX_NIC_PCIE` and hugepages

3. `run_e2e_rx_mixed_dds_container.sh` — launches DDS RX (RX2)
   - Identical pattern to `run_e2e_dds_rx_container.sh`; switch config to `test_mixed_rx_dds.yaml`
   - `--network=host`, no PCIe device binding, no hugepages

**Step 7.3: Update CMakeLists.txt**

Add all new files to the `connext_app_tests` custom target:
- `test_mixed_transport_e2e.py` (Python test)
- `run_e2e_tx_mixed_container.sh`
- `run_e2e_rx_mixed_ano_container.sh`
- `run_e2e_rx_mixed_dds_container.sh`
- `config/test_mixed_tx.yaml`
- `config/test_mixed_rx_ano.yaml`
- `config/test_mixed_rx_dds.yaml`

**Step 7.4: Test Implementation**

File: `test_mixed_transport_e2e.py`

Following SOLID principles and reusing existing `test_utils.py` abstractions:

```python
"""
Mixed-Transport End-to-End Test (ANO + DDS)

Validates that a single TX sends to two RX containers concurrently:
  - RX1: ANO (GPU Direct / DPDK) — requires physical NIC
  - RX2: DDS (standard) — no NIC required
"""
```

**Key design decisions (COPILOT.md compliance):**

- **Single Responsibility**: `MixedTransportOrchestrator` (new thin subclass or delegation layer) handles
  the 3-container lifecycle; parsing is delegated to per-transport `OutputParser` instances
- **Open/Closed**: reuse `MultiProcessOrchestrator` unchanged — configure it with heterogeneous
  script paths (two different RX scripts) rather than modifying it
- **Dependency Inversion**: inject `ANOOutputParser` for RX1 and `DDSOutputParser` for RX2 — both
  already implement the `OutputParser` interface; `MultiTopologyOutputParser` accepts a list
- **Interface Segregation**: `MultiTopologyOutputParser` already accepts `parse_multiple()` — extend
  it to accept a per-process parser list instead of a single shared parser (open/closed extension)

**Test flow:**
1. Validate environment: `TEST_TX_NIC_PCIE` + `TEST_RX_NIC_PCIE` + hugepages (needs 3 total → allocate 4)
2. Start RX2 (DDS) container first — no DPDK race condition risk
3. Wait 2s, start RX1 (ANO) container
4. Wait 5s for both receivers to initialise
5. Start TX (dual-mode) container
6. Run for `timeout_seconds` (default 90s)
7. Stop all containers, collect outputs
8. Parse RX1 output with `ANOOutputParser`, RX2 output with `DDSOutputParser`
9. Validate independently:
   - RX1 (ANO): received ≥ 80% of TX messages via ANO log pattern
   - RX2 (DDS): received ≥ 80% of TX messages via DDS log pattern

**Payload disambiguation:**

Both transports share the same `payload_source.base_payload: mixed_test_payload` prefix, so the
log pattern differs only by the parser used:
- `ANOOutputParser` matches: `PayloadSink received payload: mixed_test_payload_#(\d+)`
- `DDSOutputParser` matches: `PayloadSink received payload: mixed_test_payload_#(\d+)`

Because each parser reads from its own container's stdout, there is no ambiguity.

**Validation Criteria:**
1. TX sent ≥ 850 ANO messages AND ≥ 850 DDS messages (both paths active)
2. RX1 (ANO) received ≥ 80% of ANO-path TX messages
3. RX2 (DDS) received ≥ 80% of DDS-path TX messages
4. RX1 log contains NO DDS receive lines (transport isolation check)
5. RX2 log contains NO ANO/DPDK receive lines (transport isolation check)

**Exit Codes:**
- 0: Both transports passed threshold
- 1: Environment validation failed
- 2: ANO path (RX1) below threshold
- 3: DDS path (RX2) below threshold
- 4: Transport isolation violated (cross-contamination detected)

**Hugepage budget:**
- TX: 1 hugepage (ANO DPDK), RX1: 1 hugepage (ANO DPDK), RX2: 0 (DDS only)
- Total: 2 containers needing hugepages → allocate 4 (`N + 2` rule)

---

### Step 8: Shell Script Refactoring

**Goal:** Eliminate the duplicated boilerplate that currently exists across all 8 launcher scripts by
consolidating shared logic into a single library file. This follows the DRY principle and the
**Open/Closed** principle from COPILOT.md: existing callers stay unchanged; new topologies are
added by passing different arguments, not by copying files.

#### Current state — duplication inventory

All 8 scripts share the same blocks verbatim or near-verbatim:

| Block | Lines (approx) | Scripts that duplicate it |
|-------|---------------|--------------------------|
| License default + arch default | 2 | all 8 |
| `TEST_TX_NIC_PCIE` validation | 4 | 6 (all ANO scripts) |
| `TEST_RX_NIC_PCIE` validation | 4 | 6 (all ANO scripts) |
| License file existence check | 4 | all 8 |
| `readlink -f` for abs license path | 1 | all 8 |
| `PATH_TO_APP` definition | 1 | all 8 |
| `mkdir -p` + `chmod 777` for DPDK dir | 2 | 6 (all ANO scripts) |
| `./holohub run-container` ANO opts | 3 | 6 (all ANO scripts) |
| `./holohub run-container` DDS opts | 3 | 2 (DDS scripts) |
| Banner `echo` block | 8–10 | all 8 |

Total: ~40–45 lines duplicated across each script. With 8 scripts that is **320–360 lines of
near-identical code** that all need to be touched whenever any shared flag changes (e.g. adding a
new `--cap-add`, or changing `PATH_TO_APP`).

#### Proposed refactoring — two files replace eight

**Result after refactoring: 2 files instead of 8**

```
tests/scripts/
├── run_container_lib.sh   ← NEW: shared library (sourced, never executed directly)
└── run_container.sh       ← NEW: unified entry point (replaces all 8 scripts)
```

The 8 old scripts become **thin one-line wrappers** (or are deleted entirely if the Python
orchestrator is updated to call `run_container.sh` with arguments directly).

---

#### `run_container_lib.sh` — shared library

Contains all reusable functions. Sourced via `. "$(dirname "$0")/run_container_lib.sh"`.

```bash
#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Shared library for connext e2e container launchers.
# Source this file; do NOT execute it directly.

readonly PATH_TO_APP="/workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/cpp"

# ---------------------------------------------------------------------------
# validate_common_env [--require-nics]
#   Checks RTI_LICENSE_FILE exists.
#   With --require-nics also checks TEST_TX_NIC_PCIE and TEST_RX_NIC_PCIE.
# ---------------------------------------------------------------------------
validate_common_env() {
    RTI_LICENSE_FILE="${RTI_LICENSE_FILE:-./rti_license.dat}"
    CONNEXTDDS_ARCH="${CONNEXTDDS_ARCH:-armv8Linux4gcc7.3.0}"

    if [[ "$1" == "--require-nics" ]]; then
        _require_var TEST_TX_NIC_PCIE "PCIe address of TX NIC (e.g. 0005:03:00.0)"
        _require_var TEST_RX_NIC_PCIE "PCIe address of RX NIC (e.g. 0005:03:00.1)"
    fi

    if [[ ! -f "$RTI_LICENSE_FILE" ]]; then
        echo "ERROR: RTI license file not found: $RTI_LICENSE_FILE"
        echo "Set RTI_LICENSE_FILE or place rti_license.dat in the current directory"
        exit 1
    fi

    RTI_LICENSE_ABS=$(readlink -f "$RTI_LICENSE_FILE")
}

# ---------------------------------------------------------------------------
# print_banner <label> [<extra_key> <extra_val> ...]
#   Prints the standard ===...=== banner.
# ---------------------------------------------------------------------------
print_banner() {
    local label="$1"; shift
    echo "=================================================="
    echo "Connext E2E Test — $label"
    echo "=================================================="
    [[ -n "${TEST_TX_NIC_PCIE:-}" ]] && echo "TX NIC PCIe:  $TEST_TX_NIC_PCIE"
    [[ -n "${TEST_RX_NIC_PCIE:-}" ]] && echo "RX NIC PCIe:  $TEST_RX_NIC_PCIE"
    echo "License:      $RTI_LICENSE_FILE"
    echo "Config:       $CONFIG_FILE"
    echo "Architecture: $CONNEXTDDS_ARCH"
    while [[ $# -gt 1 ]]; do echo "$1: $2"; shift 2; done
    echo "=================================================="
}

# ---------------------------------------------------------------------------
# make_dpdk_dir <dir_name>
#   Creates and chmod 777s a DPDK runtime directory under $HOME.
# ---------------------------------------------------------------------------
make_dpdk_dir() {
    local dir="/home/$USER/$1"
    mkdir -p "$dir"
    chmod 777 "$dir"
    echo "$dir"        # returns path
}

# ---------------------------------------------------------------------------
# run_ano_container <dpdk_dir> <config_path> <label>
#   Launches connext_app with full DPDK/GPU-Direct privileges.
# ---------------------------------------------------------------------------
run_ano_container() {
    local dpdk_dir="$1" config_path="$2" label="$3"
    ./holohub run-container connext_app_cpp \
        --docker-opts="--user root \
            -v $RTI_LICENSE_ABS:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat \
            --cap-add=SYS_ADMIN --cap-add=IPC_LOCK --cap-add=NET_ADMIN \
            --device=/dev/hugepages:/dev/hugepages \
            --ulimit memlock=-1:-1 --privileged \
            -v /dev/hugepages:/dev/hugepages" \
        -- /bin/bash -c "echo '=== $label ===' && \
            $PATH_TO_APP/connext_app $config_path 2>&1 || echo 'Exit code:' \$?"
}

# ---------------------------------------------------------------------------
# run_dds_container <config_path> <label>
#   Launches connext_app with DDS-only options (no DPDK, no hugepages).
# ---------------------------------------------------------------------------
run_dds_container() {
    local config_path="$1" label="$2"
    ./holohub run-container connext_app_cpp \
        --docker-opts="--user root \
            -v $RTI_LICENSE_ABS:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat \
            -e CONNEXTDDS_ARCH=$CONNEXTDDS_ARCH \
            -e CONNEXT_CONTAINER_MODE=1 \
            -w $PATH_TO_APP" \
        -- /bin/bash -c "echo '=== $label ===' && \
            $PATH_TO_APP/connext_app $config_path 2>&1 || echo 'Exit code:' \$?"
}

# ---------------------------------------------------------------------------
# _require_var <VAR_NAME> <hint>  (internal)
# ---------------------------------------------------------------------------
_require_var() {
    if [[ -z "${!1}" ]]; then
        echo "ERROR: $1 environment variable not set"
        echo "Example: export $1=$2"
        exit 1
    fi
}
```

---

#### `run_container.sh` — unified entry point

A single dispatcher that replaces all 8 scripts. The Python orchestrator (and any
human operator) calls it with two arguments: **transport** and **role+id**.

```
Usage:
  run_container.sh ano   tx              # ANO 1-to-1 TX
  run_container.sh ano   rx              # ANO 1-to-1 RX
  run_container.sh ano   tx:1tm          # ANO 1-to-many TX
  run_container.sh ano   rx:1tm:<id>     # ANO 1-to-many RX (id = 1..3)
  run_container.sh ano   tx:mtm:<id>     # ANO many-to-many TX (id = 1..2)
  run_container.sh ano   rx:mtm:<id>     # ANO many-to-many RX (id = 1..3)
  run_container.sh ano   tx:mixed        # Mixed-transport TX (ANO+DDS)
  run_container.sh ano   rx:mixed        # Mixed-transport RX1 (ANO)
  run_container.sh dds   tx              # DDS 1-to-1 TX
  run_container.sh dds   rx              # DDS 1-to-1 RX
  run_container.sh dds   rx:mixed        # Mixed-transport RX2 (DDS)
```

Internally it sources `run_container_lib.sh`, resolves the config path and DPDK dir
name from the arguments, then calls `run_ano_container` or `run_dds_container`.
The routing table is a `case` statement — adding a new topology is **one new `case` arm**,
not a new file.

```bash
#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/run_container_lib.sh"

TRANSPORT="${1:-}"
ROLE="${2:-}"

if [[ -z "$TRANSPORT" || -z "$ROLE" ]]; then
    echo "Usage: $0 <transport> <role>"
    echo "  transport: ano | dds"
    echo "  role:      tx | rx | tx:<topology>[:<id>] | rx:<topology>[:<id>]"
    exit 1
fi

case "$TRANSPORT:$ROLE" in
    # ------------------------------------------------------------------
    # ANO paths (require NICs + hugepages)
    # ------------------------------------------------------------------
    ano:tx)
        validate_common_env --require-nics
        CONFIG_FILE="config/test_ano_tx.yaml"
        print_banner "ANO 1-to-1 TX"
        DPDK_DIR=$(make_dpdk_dir dpdk_tx)
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "ANO TX"
        ;;
    ano:rx)
        validate_common_env --require-nics
        CONFIG_FILE="config/test_ano_rx.yaml"
        print_banner "ANO 1-to-1 RX"
        DPDK_DIR=$(make_dpdk_dir dpdk_rx)
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "ANO RX"
        ;;
    ano:tx:1tm)
        validate_common_env --require-nics
        CONFIG_FILE="config/test_ano_tx_1_to_many.yaml"
        print_banner "ANO 1-to-Many TX"
        DPDK_DIR=$(make_dpdk_dir dpdk_tx_1tm)
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "ANO 1-to-Many TX"
        ;;
    ano:rx:1tm:*)
        ID="${ROLE##*:}"
        validate_common_env --require-nics
        CONFIG_FILE="config/test_ano_rx_1_to_many_sub${ID}.yaml"
        print_banner "ANO 1-to-Many RX Sub${ID}" "UDP Port" "500${ID}"
        DPDK_DIR=$(make_dpdk_dir "dpdk_rx_sub${ID}")
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "ANO 1-to-Many RX Sub${ID}"
        ;;
    ano:tx:mtm:*)
        ID="${ROLE##*:}"
        validate_common_env --require-nics
        CONFIG_FILE="config/test_ano_tx_many_to_many_pub${ID}.yaml"
        print_banner "ANO Many-to-Many TX Pub${ID}" "UDP Port" "600${ID}"
        DPDK_DIR=$(make_dpdk_dir "dpdk_tx_pub${ID}")
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "ANO Many-to-Many TX Pub${ID}"
        ;;
    ano:rx:mtm:*)
        ID="${ROLE##*:}"
        validate_common_env --require-nics
        CONFIG_FILE="config/test_ano_rx_many_to_many_sub${ID}.yaml"
        print_banner "ANO Many-to-Many RX Sub${ID}" "UDP Port" "600${ID}"
        DPDK_DIR=$(make_dpdk_dir "dpdk_rx_mtm_sub${ID}")
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "ANO Many-to-Many RX Sub${ID}"
        ;;
    ano:tx:mixed)
        validate_common_env --require-nics
        CONFIG_FILE="config/test_mixed_tx.yaml"
        print_banner "Mixed-Transport TX (ANO+DDS)"
        DPDK_DIR=$(make_dpdk_dir dpdk_tx_mixed)
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "Mixed TX"
        ;;
    ano:rx:mixed)
        validate_common_env --require-nics
        CONFIG_FILE="config/test_mixed_rx_ano.yaml"
        print_banner "Mixed-Transport RX1 (ANO)"
        DPDK_DIR=$(make_dpdk_dir dpdk_rx_mixed)
        run_ano_container "$DPDK_DIR" "$PATH_TO_APP/tests/$CONFIG_FILE" "Mixed RX1 (ANO)"
        ;;
    # ------------------------------------------------------------------
    # DDS paths (no NICs, no hugepages)
    # ------------------------------------------------------------------
    dds:tx)
        validate_common_env
        CONFIG_FILE="config/test_dds_tx.yaml"
        print_banner "DDS 1-to-1 TX" "Transport" "DDS (domain=42)"
        run_dds_container "$PATH_TO_APP/tests/$CONFIG_FILE" "DDS TX"
        ;;
    dds:rx)
        validate_common_env
        CONFIG_FILE="config/test_dds_rx.yaml"
        print_banner "DDS 1-to-1 RX" "Transport" "DDS (domain=42)"
        run_dds_container "$PATH_TO_APP/tests/$CONFIG_FILE" "DDS RX"
        ;;
    dds:rx:mixed)
        validate_common_env
        CONFIG_FILE="config/test_mixed_rx_dds.yaml"
        print_banner "Mixed-Transport RX2 (DDS)" "Transport" "DDS (domain=42)"
        run_dds_container "$PATH_TO_APP/tests/$CONFIG_FILE" "Mixed RX2 (DDS)"
        ;;
    *)
        echo "ERROR: Unknown transport:role combination: $TRANSPORT:$ROLE"
        exit 1
        ;;
esac
```

---

#### Compatibility wrapper (backward-compatible migration path)

The 8 old scripts are replaced by one-line wrappers that delegate to `run_container.sh`.
This keeps the Python orchestrator and any existing CI invocations working without change
until a follow-up PR updates the callers:

```bash
# run_e2e_tx_container.sh  (example — same pattern for all 8)
#!/usr/bin/env bash
exec "$(dirname "$0")/run_container.sh" ano tx "$@"
```

Once the Python orchestrator is updated to call `run_container.sh` directly, the wrappers
can be deleted.

#### Migration steps

1. **Create** `tests/scripts/run_container_lib.sh` with the library functions
2. **Create** `tests/scripts/run_container.sh` with the dispatcher
3. **Verify** each old script still passes its manual smoke-test by running it (it will delegate)
4. **Update** `MultiProcessOrchestrator` in `test_utils.py`: replace the 8 `TestScriptPaths`
   entries with calls to `run_container.sh <transport> <role>` — this is the only Python
   change needed
5. **Update** `CMakeLists.txt` to copy `run_container_lib.sh` and `run_container.sh` instead
   of (or in addition to) the individual scripts until the wrappers are retired
6. **Delete** the 8 individual scripts in a follow-up PR once CI confirms the wrappers work

#### Impact summary

| Metric | Before | After |
|--------|--------|-------|
| Shell script files | 8 | 2 (+ 8 one-line wrappers, temporary) |
| Lines of duplicated boilerplate | ~350 | 0 |
| Adding a new topology | New file (copy-paste) | One new `case` arm in `run_container.sh` |
| Changing a Docker flag | 8 files to edit | 1 function in `run_container_lib.sh` |
| Python orchestrator changes | None (backward-compat) | Update script path strings (optional) |

## Testing Strategy

### Test Pyramid Approach

Following the writing-tests skill:

- **Unit Tests** (future): Individual components (parsers, validators) in isolation
- **Integration Tests**: Current scope - multiple containers working together
- **End-to-End Tests**: Full system validation with physical hardware

### Test Coverage

**What These Tests Validate:**
- ✅ Multi-subscriber support with port routing
- ✅ Multi-publisher concurrent transmission
- ✅ DPDK flow rule configuration correctness
- ✅ GPU-Direct memory management under load
- ✅ Container orchestration and resource isolation
- ✅ Message delivery reliability (80% threshold)
- ✅ Dual-mode TX (ANO + DDS simultaneously) via `ConnextTxOp`
- ✅ Transport isolation (RX1 only ANO, RX2 only DDS, no cross-contamination)

**What These Tests Don't Validate:**
- ❌ Individual function/class behavior (needs unit tests)
- ❌ Performance benchmarks (use adv_networking_bench)
- ❌ Failure recovery scenarios
- ❌ Maximum scalability limits

### Validation Criteria

**Success Criteria for 1-to-Many:**
1. TX sends at least 850 messages (90s × 10 msg/sec)
2. Each of N subscribers receives ≥ 680 messages (80% threshold)
3. Sum of all subscriber messages ≤ N × TX messages sent
4. No duplicate message IDs across subscribers

**Success Criteria for Many-to-Many:**
1. Each of M publishers sends at least 850 messages
2. Each subscriber receives ≥ 80% from each configured publisher
3. Publisher attribution is correct (payload prefix matching)
4. No messages from non-configured publishers appear in subscriber output
5. Total system message delivery rate acceptable

**Success Criteria for Mixed-Transport (ANO + DDS):**
1. TX activates both transports and sends ≥ 850 messages on each path
2. RX1 (ANO) receives ≥ 80% of TX messages via DPDK GPU Direct
3. RX2 (DDS) receives ≥ 80% of TX messages via DDS
4. No ANO packets appear in RX2 log; no DDS samples appear in RX1 log
5. Both validations are independent (either can fail independently)

## Risks and Mitigations

### Risk: Resource Exhaustion

**Impact:** High - Could cause test failures or system instability

**Mitigations:**
- Pre-validate hugepage availability
- Add resource checks before starting containers
- Implement configurable test scale (via command-line arguments)
- Validate requested topology against system capabilities
- Document minimum system requirements

### Risk: Container Timing Issues

**Impact:** Medium - Race conditions during startup

**Mitigations:**
- Stagger container startup (2s for RX, 1s for TX)
- Add health checks before proceeding
- Implement timeout mechanisms
- Log detailed timing information

### Risk: DPDK Flow Rule Conflicts

**Impact:** High - Messages routed to wrong subscribers

**Mitigations:**
- Use non-overlapping port ranges
- Validate flow configuration in RX startup logs
- Add explicit flow rule validation in test
- Document port assignment strategy clearly

### Risk: Message Duplication/Loss

**Impact:** Medium - Test false positives/negatives

**Mitigations:**
- Use unique message IDs (payload sequence numbers)
- Track message IDs per subscriber
- Add validation for duplicate detection
- Implement detailed logging of message flow

## Success Metrics

**Test Suite Quality:**
- All tests pass consistently (>95% success rate)
- Test execution time reasonable (<5 minutes per test)
- Clear failure messages for debugging
- No false positives/negatives

**Code Quality:**
- Follow SOLID principles (maintainable, extensible)
- Comprehensive documentation (README, docstrings)
- Consistent with existing test patterns
- No code duplication (DRY principle)

**Coverage:**
- Both topologies tested automatically
- Key failure scenarios documented
- Troubleshooting guide complete

## Future Enhancements

1. **Extended Command-Line Interface**
   - Custom port range specification via CLI
   - JSON/YAML file defining complex topologies
   - Per-publisher/subscriber configuration overrides
   - Arbitrary N publishers, M subscribers (with validation)

2. **Performance Metrics**
   - Measure throughput (Gbps)
   - Track latency (p50, p99)
   - Monitor resource utilization

3. **Failure Injection**
   - Simulate container crashes
   - Network packet loss scenarios
   - Resource starvation conditions


## References

- **Existing Tests:** `test_ano_e2e.py`, `test_dds_e2e.py`
- **Multi-Topology Tests:** `test_ano_1_to_many_e2e.py`, `test_ano_many_to_many_e2e.py`
- **Mixed-Transport Test (planned):** `test_mixed_transport_e2e.py`
- **Test Utilities:** `test_utils.py` (`ANOOutputParser`, `DDSOutputParser`, `MultiProcessOrchestrator`, `MultiTopologyOutputParser`)
- **Guidelines:** `.ia/COPILOT.md`, `.ia/skills/writing-tests/`
- **Application:** `connext_app_cpp/cpp/main.cpp`
- **Configurations:** `config/test_ano_tx.yaml`, `config/test_ano_rx.yaml`, `config/test_dds_tx.yaml`, `config/test_dds_rx.yaml`
- **Advanced Networking:** `operators/advanced_network/README.md`
- **Operator Architecture:** `operators/connext/README.md` (dual-transport `ConnextTxOp` design)

## Approval and Sign-off

**Created:** February 23, 2026

**Last Updated:** February 25, 2026 (Step 7 Mixed-Transport ANO+DDS COMPLETE and passing)

**Status:** ✅ Steps 1–7 COMPLETE | ⏳ Step 8 (Script Refactoring) PENDING

### Implementation Progress

#### ✅ Completed Steps

**Step 1: Configuration Files** - ✅ COMPLETE (Feb 24, 2026)
- ✅ `test_ano_tx_1_to_many.yaml` - TX configuration with unified `ano_channel`
- ✅ `test_ano_rx_1_to_many_sub1.yaml` - RX1 on port 5001, unified `ano_channel`
- ✅ `test_ano_rx_1_to_many_sub2.yaml` - RX2 on port 5002, unified `ano_channel`
- ✅ `test_ano_rx_1_to_many_sub3.yaml` - RX3 on port 5003, unified `ano_channel`
- **Critical Fix Applied**: All configs now use `ano_channel: "E2ETestChannel_1toMany"` (unified)
- **Unique Identifiers**: `ano_buffer_id` different per RX (e2e_test_buffer_1tm_sub1/2/3)

**Step 1.5: Update CMakeLists.txt** - ✅ COMPLETE (Feb 24, 2026)
- ✅ Added all 1-to-many test files to `connext_app_tests` target
- ✅ Python test file: `test_ano_1_to_many_e2e.py`
- ✅ Shell scripts: `run_e2e_tx_1_to_many_container.sh`, `run_e2e_rx_1_to_many_container.sh`
- ✅ YAML configs: All 4 configuration files registered

**Step 2: Shell Scripts** - ✅ COMPLETE (Feb 24, 2026)
- ✅ `run_e2e_tx_1_to_many_container.sh` - TX launcher (parameterless)
- ✅ `run_e2e_rx_1_to_many_container.sh` - RX launcher (takes subscriber ID parameter)
- ✅ Unique DPDK runtime directories per container
- ✅ Proper error handling and validation

**Step 3: Extended Test Utilities** - ✅ COMPLETE (Feb 24, 2026)
- ✅ `MultiProcessOrchestrator` - Manages N RX + M TX processes
- ✅ Hugepage validation before each container launch
- ✅ Per-container resource checking
- ✅ 2-second delays between RX containers (prevents hugepage race condition)
- ✅ 5-second wait after all RX started (allows DPDK initialization)
- ✅ Container lifecycle management with proper cleanup

**Step 4: Test Implementation - 1-to-Many** - ✅ COMPLETE (Feb 24, 2026)
- ✅ `test_ano_1_to_many_e2e.py` - Full 1-to-3 test implementation
- ✅ Command-line arguments for customization
- ✅ Environment validation (NICs, hugepages)
- ✅ Topology validation
- ✅ Multi-container orchestration with timing controls
- ✅ Output parsing and validation
- ✅ **TEST VERIFIED PASSING** - All 3 RX received messages after channel fix

**Critical Bug Fix (Feb 24, 2026):**
- **Root Cause**: ANO discovery uses `ano_channel` as filter in `DdsIdlSenderResourcesManager::processSample()`
- **Problem**: Original configs had different channels (E2ETestChannel_1toMany_Sub1/2/3)
- **Impact**: TX filtered out all RX announcements → 0 messages delivered
- **Solution**: Unified `ano_channel: "E2ETestChannel_1toMany"` across all 4 configs
- **Result**: TX now discovers all 3 RX endpoints, messages delivered successfully

**Documentation Updates:**
- ✅ `operators/connext/README.md` - Added "ANO Channel vs Buffer ID" section
- ✅ Documented discovery mechanism (control plane on domain 101)
- ✅ Clarified `enable_dds` vs `enable_ano` configuration modes
- ✅ Explained channel filter matching requirement

**Step 5: Test Implementation - Many-to-Many** - 🔄 IN PROGRESS (Feb 25, 2026)

**Topology Change (Feb 25):** Reduced from 4 RX to 3 RX subscribers
- ✅ Created 2 TX publisher configuration files (pub1, pub2)
- ✅ Updated to 3 RX subscriber configuration files (sub1-3) - **removed sub4**
- ✅ Publisher 1 (port 6001) with payload prefix "pub1_payload"
- ✅ Publisher 2 (port 6002) with payload prefix "pub2_payload"
- ✅ Subscriber 1: receives from Pub1 and Pub2 (ports 6001, 6002) - dual flow
- ✅ Subscriber 2: receives from Pub1 only (port 6001) - single flow
- ✅ Subscriber 3: receives from Pub2 only (port 6002) - single flow
- ✅ Created TX launcher script: `run_e2e_tx_many_to_many_container.sh`
- ✅ Created RX launcher script: `run_e2e_rx_many_to_many_container.sh`
- ✅ Updated CMakeLists.txt with all many-to-many files
- ✅ Implemented `test_ano_many_to_many_e2e.py` with 3-subscriber topology
- ✅ Updated default num_subscribers from 4 to 3
- ✅ Updated publisher-subscriber mapping: Pub1→[1,2], Pub2→[1,3]
- ✅ Added payload attribution validation (pub1/pub2 prefix matching)
- ✅ Updated timing: rx_stagger=5s, tx_stagger=5s, rx_init_wait=10s
- ✅ Created diagnostic script: `check_gpu_dpdk_state.sh`

**Many-to-One Intermediate Step (Feb 25, 2026) — ✅ PASSING:**

Before validating the full 3-subscriber topology, a simpler 2TX→1RX
(many-to-one) configuration was used to isolate and debug the dual-publisher
reception path. This intermediate step **PASSES** with near-perfect throughput:

```
Parsing TX Publisher 1:   Sent: 1008 messages (prefix 'pub1_payload')
Parsing TX Publisher 2:   Sent:  938 messages (prefix 'pub2_payload')
Parsing RX Subscriber 1:  Received: 1944 messages (all publishers)

Subscriber 1:
  Pub1: received 1007/1008  (99.9%,  min=806)  ✓ OK
  Pub2: received  937/938   (99.9%,  min=750)  ✓ OK

✓ TEST PASSED - All subscribers met threshold
```

**Key Fixes That Unblocked the 2TX→1RX Test (Feb 25, 2026):**

1. **`kAnoReaderPollInterval` reduced from 100ms → 10ms** (in `connext_rx.cpp`)  
   The hardcoded 100ms timeout caused `readNext()` to block the Holoscan scheduler
   thread, preventing it from draining the DPDK RX queue fast enough. Changing to
   10ms allows ~100 drain attempts/sec instead of ~10.
   > ⚠️ Note: This change was applied **directly** to the hardcoded constant in
   > `connext_rx.cpp`. Task 1 in `operators/connext/TASK.md` tracks making this
   > value YAML-configurable (`ano_poll_timeout_ms`) as a proper follow-up.

2. **`rx_meta_buffers: 2048`** added to `test_ano_rx_many_to_many_sub1.yaml`  
   Raised DPDK metadata buffer pool from default 256 → 2048 to prevent pool
   exhaustion when two publishers feed the same queue simultaneously.

3. **`batch_size: 256`** on `rx_q_0` (reduced from 10240)  
   At 10 msg/sec per publisher, batches of 10240 never fill naturally and were
   only flushed by `timeout_us: 1000` (1ms), creating ~1000 burst allocations/sec
   against a pool of 256. Smaller batch reduces meta buffer accumulation rate.

4. **`message_period_ms: 10`** in `demo` section (reduced from 100ms)  
   Holoscan now calls `ConnextRxOp::compute()` 10× more often, draining the
   DPDK ring buffer faster and preventing meta buffer pool exhaustion.

5. **DPDK flow rules**: Two flow rules with mandatory `udp_src` + `udp_dst`
   matching, both routing to queue 0:  
   - Rule 1: `udp_src:6001 + udp_dst:6001` → queue 0 (Pub1)
   - Rule 2: `udp_src:6002 + udp_dst:6001` → queue 0 (Pub2)

**Many-to-Two Intermediate Step (Feb 25, 2026) — ✅ PASSING:**

With 2TX→1RX passing at 99.9%, the next iterative step was **2TX→2RX** where
**both** subscribers receive from **both** publishers. This isolated whether the
full dual-flow-rule configuration works for two simultaneous RX instances before
adding the asymmetric routing required by the final 2TX→3RX topology.
This step **PASSES** with all subscribers well above threshold:

```
Parsing TX Publisher 1:   Sent: 1307 messages (prefix 'pub1_payload')
Parsing TX Publisher 2:   Sent: 1237 messages (prefix 'pub2_payload')
Parsing RX Subscriber 1:  Received: 2481 messages (all publishers)
Parsing RX Subscriber 2:  Received: 2212 messages (all publishers)

Subscriber 1:
  Pub1: received 1300/1307  (99.5%,  min=1045)  ✓ OK
  Pub2: received 1181/1237  (95.5%,  min=989)   ✓ OK

Subscriber 2:
  Pub1: received 1125/1307  (86.1%,  min=1045)  ✓ OK
  Pub2: received 1087/1237  (87.9%,  min=989)   ✓ OK

Total messages received (all subs, all pubs): 4693

✓ TEST PASSED - All subscribers met threshold
```

**Topology:**
```
TX1 (src 6001) ──┬──> RX1 (dst 6001, rules: src 6001 + src 6002)
                 └──> RX2 (dst 6002, rules: src 6001 + src 6002)

TX2 (src 6002) ──┬──> RX1 (dst 6001, rules: src 6001 + src 6002)
                 └──> RX2 (dst 6002, rules: src 6001 + src 6002)
```

**Why this step before 2TX→3RX:**
- Adds one extra container (RX2) relative to the passing 2TX→1RX
- Both RX are symmetric (same dual-flow-rule pattern), isolating container
  scaling issues from routing correctness issues
- Keeps resource footprint smaller than the final 3-subscriber run
- Validates that two concurrent DPDK stacks can each handle dual-publisher
  ingress without interfering with each other

**Configuration Changes Required:**

1. **`test_ano_rx_many_to_many_sub2.yaml`** — repurposed for 2TX→2RX:
   - `ano_fast_port`: changed from `6001` to `6002` (RX2 listens on its own dst port)
   - `ano_buffer_id`: `e2e_test_buffer_m2m_sub2` (unchanged)
   - DPDK flow rules:
     - Rule 1: `udp_src: 6001, udp_dst: 6002` → queue 0 (Pub1 → RX2)
     - Rule 2: `udp_src: 6002, udp_dst: 6002` → queue 0 (Pub2 → RX2)
   - `rx_meta_buffers: 2048` (same as sub1 — dual publisher ingress)
   - `batch_size: 256` (same as sub1)
   - `message_period_ms: 10` (same as sub1)
   - `ano_channel: "E2ETestChannel_ManyToMany"` (same as all other m2m configs)
   - `payload_sink.expected_payload_prefix: pub` (accepts pub1 and pub2)

2. **TX configurations** (`pub1.yaml`, `pub2.yaml`) — **no change needed**:
   - Each TX publisher sends to its own `ano_fast_port` (Pub1 → 6001, Pub2 → 6002).
   - ANO discovery (DDS domain 101) announces each TX endpoint; subscribed RX instances
     register their `ano_buffer_id` independently and receive from any matching TX on the channel.
   - The DPDK flow rules on each RX side use `udp_src` (publisher src port) to distinguish
     traffic from Pub1 vs Pub2 arriving at the same `udp_dst` (this RX's `ano_fast_port`).
   - ✅ **Resolved**: No TX config changes required; RX-side flow rules are sufficient.

3. **Shell scripts** — `run_e2e_rx_many_to_many_container.sh` already accepts
   subscriber ID `$1`; sub2 config changes above are the only script-visible
   difference.

**Confirmed Observations (2TX→2RX):**
- RX1 reception rate: Pub1 99.5%, Pub2 95.5% — well above 80% threshold
- RX2 reception rate: Pub1 86.1%, Pub2 87.9% — above 80% threshold (lower due to
  higher container count competing for hugepages, but still passing)
- No cross-contamination observed between RX1 and RX2
- `rx_meta_buffers: 2048` sufficient for both RX containers simultaneously
- RX2 receives slightly fewer messages than RX1 due to the second container
  adding resource pressure, but both remain comfortably above the 80% threshold

#### ✅ Step 5 COMPLETE / Remaining Steps

**Step 5: Test Implementation - Many-to-Many** - ✅ COMPLETE (Feb 25, 2026)
- ✅ Infrastructure ready (scripts, configs, CMakeLists.txt, Python test file)
- ✅ Many-to-one intermediate (2TX→1RX) validated at 99.9%
- ✅ **Many-to-two intermediate (2TX→2RX) PASSING** — all subscribers ≥80% per publisher
- ✅ **Full 3-subscriber run (2TX→3RX) PASSING** — all subscribers 99.9% per publisher

**Full Many-to-Many Step (Feb 25, 2026) — ✅ PASSING:**

With 2TX→2RX passing, the final step was **2TX→3RX** — the asymmetric full
many-to-many topology where Sub1 receives from both publishers, Sub2 only from
Pub1, and Sub3 only from Pub2. This step **PASSES** with near-perfect 99.9%
throughput on all subscribers:

```
Parsing TX Publisher 1:   Sent: 1307 messages (prefix 'pub1_payload')
Parsing TX Publisher 2:   Sent: 1239 messages (prefix 'pub2_payload')
Parsing RX Subscriber 1:  Received: 2544 messages (all publishers)
Parsing RX Subscriber 2:  Received: 1306 messages (all publishers)
Parsing RX Subscriber 3:  Received: 1238 messages (all publishers)

Subscriber 1:
  Pub1: received 1306/1307  (99.9%,  min=1045)  ✓ OK
  Pub2: received 1238/1239  (99.9%,  min=991)   ✓ OK

Subscriber 2:
  Pub1: received 1306/1307  (99.9%,  min=1045)  ✓ OK
  Pub2: received    0/1239  ( 0.0%  — correct, not expected)  ✓

Subscriber 3:
  Pub1: received    0/1307  ( 0.0%  — correct, not expected)  ✓
  Pub2: received 1238/1239  (99.9%,  min=991)   ✓ OK

Total messages received (all subs, all pubs): 5088

✓ TEST PASSED - All subscribers met threshold
```

**Topology verified:**
```
TX1 (src 6001) ──┬──> RX1 (dst 6001, dual-flow: src 6001 + src 6002)  [Pub1+Pub2]
                 └──> RX2 (dst 6002, single-flow: src 6001)             [Pub1 only]

TX2 (src 6002) ──┬──> RX1 (dst 6001, dual-flow: src 6001 + src 6002)  [Pub1+Pub2]
                 └──> RX3 (dst 6003, single-flow: src 6002)             [Pub2 only]
```

**Key Fix That Unblocked 2TX→3RX — `num_bufs` reduction:**

The TX container failed with a GPU DMA mapping error when launching alongside
3 RX containers (5 containers total):
```
mlx5_common: Fail to create MR for address (0xffff60200000)
mlx5_common: Device 0005:03:00.0 unable to DMA map
[critical] Could not DMA map EXT memory: -1 err=Invalid argument
```
With only 4 containers (2TX+2RX) the same code worked fine.

Root cause and fix documented in **Lesson 6** (SMMU Address Space Exhaustion)
in the Lessons Learned section below.

**Step 6: Documentation Updates** - ✅ COMPLETE (Feb 25, 2026)
- ✅ Updated `tests/README.md` with full Multi-Topology ANO Tests section
- ✅ Added ASCII topology diagrams for 1-to-many and many-to-many
- ✅ Documented hugepage and SMMU troubleshooting
- ✅ Added pre-flight checklist and updated Table of Contents

**Step 7: Mixed-Transport End-to-End Test (ANO + DDS)** - ✅ COMPLETE (Feb 25, 2026)
- ✅ `config/test_mixed_tx.yaml` — dual-mode TX (`enable_ano: true` + `enable_dds: true`), port 7000, topic `E2ETestTopicMixed`, domain 42, `num_bufs: 16384`
- ✅ `config/test_mixed_rx_ano.yaml` — ANO-only RX, port 7000, channel `E2ETestChannelMixed`, `num_bufs: 16384`
- ✅ `config/test_mixed_rx_dds.yaml` — DDS-only RX, no `advanced_network` block, topic `E2ETestTopicMixed`
- ✅ `scripts/run_e2e_tx_mixed_container.sh` — dual-mode TX launcher (ANO docker opts)
- ✅ `scripts/run_e2e_rx_mixed_ano_container.sh` — ANO RX launcher (ANO docker opts)
- ✅ `scripts/run_e2e_rx_mixed_dds_container.sh` — DDS RX launcher (DDS-only opts, no hugepages)
- ✅ `CMakeLists.txt` updated: all 7 new files in both COMMAND and DEPENDS sections
- ✅ `test_mixed_transport_e2e.py` implemented with `MixedANOOutputParser`, `MixedDDSOutputParser`, `MixedTransportOrchestrator`, and tightened transport isolation check
- ✅ `tests/README.md` updated with Mixed-Transport section (topology diagram, config table, validation details)
- ✅ C++ dual-transport support implemented (see Step 7.0 and `applications/connext/TASK.md` Task 1):
  - Removed mutual-exclusion guard from `ConnextTxOp::start()`
  - Dual-dispatch in `compute()`: ANO (GPU direct) + DDS (GPU→CPU copy) per tick
  - Fixed `use_gpu` derivation and `adv_net_init()` gate in `connext_common.cpp`
  - Fixed false-positive transport isolation keywords in `test_mixed_transport_e2e.py`
- ✅ **TEST VERIFIED PASSING** — RX1 (ANO) and RX2 (DDS) both ≥ 80%, isolation confirmed

### Lessons Learned

1. **ANO Discovery Architecture**:
   - Discovery always uses DDS domain 101 (hardcoded `kDiscoveryDomainId`)
   - `ano_channel` is a **discovery filter**, not a unique identifier
   - TX only accepts RX announcements with matching `ano_channel`
   - `ano_buffer_id` provides unique endpoint identification

2. **Hugepage Resource Management**:

   **Requirements per topology:**

   | Test | Containers | Hugepages needed | Recommended total |
   |------|-----------|-----------------|-------------------|
   | 1-to-1 (`test_ano_e2e`) | 2 | 2 | 3 |
   | 1-to-many (3 RX) | 4 | 4 | 5 |
   | many-to-many (2TX+3RX) | 5 | 5 | 7 |
   | mixed-transport (ANO+DDS) | 2 (TX + RX1-ANO; RX2-DDS needs 0) | 2 | 4 |

   **General rule:** allocate `N + 2` hugepages where `N` is the total number of containers.

   **Checking current state:**
   ```bash
   cat /proc/meminfo | grep HugePages
   # HugePages_Total:  7   ← allocated
   # HugePages_Free:   0   ← available right now
   ```

   **Symptom — not enough hugepages allocated:**
   ```
   EAL: No free 1048576 kB hugepages reported on node 0
   EAL: FATAL: Cannot get hugepage information.
   [critical] Failed to initialize DPDK
   ```
   This typically fails on the **last container** launched (usually TX), because all
   hugepages were consumed by previously started RX containers.

   **Fix — allocate more hugepages (temporary, until reboot):**
   ```bash
   sudo sh -c 'echo 7 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages'
   cat /proc/meminfo | grep HugePages_Total   # verify
   ```

   **Fix — permanent (survives reboot), add to `/etc/sysctl.conf`:**
   ```
   vm.nr_hugepages = 7
   ```
   Then apply: `sudo sysctl -p`

   **Fix — allocation fails (memory fragmentation):**
   ```bash
   # Drop page cache to consolidate free memory, then retry
   sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
   sudo sh -c 'echo 7 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages'
   ```
   Allocate hugepages **early after boot** before the kernel fragments memory.

   **Symptom — hugepages allocated but containers still fail (Exit code 137):**
   The test framework waits 2 seconds after each container launch before proceeding, because
   Docker takes ~1–2 s to actually reserve hugepages. If this race is hit:
   - Check `HugePages_Free` drops after each container starts
   - Verify `/dev/hugepages` is accessible inside containers: `ls -la /dev/hugepages`
   - Check hugepages are mounted: `mount | grep hugepages`

   **Symptom — hugepages not freed after test (stale containers):**
   ```bash
   # Kill stale containers so hugepages are released
   docker rm -f $(docker ps -aq --filter ancestor=holohub:connext_app_cpp)
   # Then verify free count recovered
   cat /proc/meminfo | grep HugePages_Free
   ```

   See also: `tests/HUGEPAGES.md` for the full reference guide.

3. **Configuration Best Practices**:
   - ✅ Use unified `ano_channel` for discovery filtering
   - ✅ Use unique `ano_buffer_id` per endpoint
   - ✅ Unique UDP ports per subscriber (5001, 5002, 5003 for 1-to-many; 6001, 6002, 6003 for many-to-many)
   - ✅ DPDK flow rules for hardware-level filtering
   - ✅ Multiple flow rules per RX when receiving from multiple TX sources

4. **GPU DMA and System State Management**:
   - GPU/DPDK driver state can become corrupted from failed test runs
   - Stale Docker containers hold hugepages and prevent cleanup
   - DPDK runtime directories persist between test runs
   - **Critical**: Always verify system state before running tests:
     ```bash
     bash applications/connext/connext_app_cpp/tests/check_gpu_dpdk_state.sh
     ```
   - **Cleanup Required**:
     1. Kill all stale Docker containers: `docker rm -f $(docker ps -aq | grep connext_app_cpp)`
     2. Clean DPDK runtime dirs: `rm -rf ~/dpdk_*`
     3. Verify hugepages freed: `cat /sys/.../hugepages-1048576kB/free_hugepages`
     4. If GPU DMA errors persist, system reboot required

5. **DPDK Meta Buffer Exhaustion (Many-to-One / Many-to-Many)**:
   - Default `rx_meta_buffers: 256` is insufficient for dual-publisher scenarios
   - With `timeout_us: 1000`, DPDK allocates ~1000 burst structs/sec; 256 exhausts in ~256ms
   - Three YAML levers to tune (applied in `test_ano_rx_many_to_many_sub1.yaml`):
     - `rx_meta_buffers: 2048` — increase pool size (at `advanced_network.cfg` level)
     - `batch_size: 256` — reduce burst accumulation rate
     - `demo.message_period_ms: 10` — drain queue 10× more often
   - The `kAnoReaderPollInterval` (timeout inside `readNext()`) must be ≤ `message_period_ms`
     to avoid stalling the Holoscan scheduler; currently hardcoded to 10ms in `connext_rx.cpp`
   - See `operators/connext/TASK.md` Task 1 for making this YAML-configurable

6. **SMMU Address Space Exhaustion on IGX Orin (Many-to-Many)**:
   - IGX Orin is an integrated SoC: GPU + both NICs share **one SMMU**
   - Each `kind: device` memory region is mapped through the SMMU via `ibv_reg_mr`
   - Total SMMU footprint = `num_bufs × adj_buf_size × num_containers` across ALL NICs
   - At `num_bufs: 51200` × ~1192 bytes ≈ 61 MB/container; 5 containers = ~305 MB → fails
   - Reducing to `num_bufs: 16384` (~19.5 MB/container; 5 × 19.5 MB = ~97.5 MB) → succeeds
   - **Rule of thumb**: keep `num_bufs × adj_size × num_containers < 200 MB` on IGX Orin
   - Hard floor: `num_bufs ≥ default_num_rx_desc = 8192` (enforced by `adv_network_dpdk_mgr.h`)
   - Symptom: `mlx5_common: Fail to create MR` + `Could not DMA map EXT memory: -1 err=Invalid argument`
   - Misleading because `rte_extmem_register` succeeds (DPDK bookkeeping) but `rte_dev_dma_map` fails (hardware)

7. **DPDK Flow Rules for Multi-Source RX**:
   - Both `udp_src` and `udp_dst` filters are mandatory in the DPDK flow rules
   - To receive from multiple publishers into one queue: use separate flow rules per
     publisher, each with its own `udp_src` value, both with `action.queue.id: 0`
   - Routing both publishers to different queues (queue 0 and queue 1) does NOT work
     because `GpuDirectNetworkReceiver` only polls the single `ano_queue_id` configured

8. **Dual-Transport TX Pitfalls (Mixed ANO + DDS)**:
   - `ConnextTxOp` had a mutual-exclusion guard that threw when both `enable_ano` and `enable_dds` were `true` — must be removed and replaced with independent `if` blocks
   - `ConnextDemoApp::compose()` gated `adv_net_init()` on `!use_dds`; in dual-transport mode `use_dds == true` caused the ANO manager to never initialise, leading to an assertion failure: `g_ano_mgr != nullptr`
   - `configure_tx_operators()` derived `use_gpu` from `!use_dds` — wrong in dual-transport mode; must derive it from `use_ano` explicitly (requires a separate `bool use_ano` field in `DemoAppConfig`)
   - In `compute()`, the ANO path must run **before** the DDS path so the GPU tensor is still live when the `cudaMemcpy` for the DDS copy executes
   - Full implementation details: `applications/connext/TASK.md` Task 1

9. **False-Positive Transport Isolation Detection**:
   - `ConnextRxOp` logs its full DDS configuration (domain, topic name, RTI version) at startup **even when `enable_dds: false`** — keywords like `'DDS'`, `'connext'`, `'RTI '` appear in every container's init log
   - Shell launcher scripts print an environment check that mentions `hugepage` **even for DDS-only containers** that never touch hugepages
   - Isolation keywords must be **runtime-only** indicators that only appear during active data exchange:
     - DDS runtime: `'DDS DataWriter'`, `'DDS DataReader'`, `'dds_topic'`, `'domain_id'`
     - ANO runtime: `'DPDK'`, `'advanced_network'`, `'ANO'`, `'PCI:'`, `'Port 0:'`
   - Avoid broad terms (`'DDS'`, `'connext'`, `'hugepage'`) that match init/setup logs on both sides

### Next Immediate Actions

1. ✅ ~~**Complete Step 6: Documentation Updates**~~ — DONE (Feb 25, 2026)

2. ✅ ~~**Implement Step 7: Mixed-Transport E2E Test**~~ — DONE (Feb 25, 2026)
   - C++ dual-transport implementation complete; test passes with RX1 (ANO) + RX2 (DDS) both ≥ 80%

3. **Implement Step 8: Shell Script Refactoring** (Priority: MEDIUM — now the next priority):
   - Create `tests/scripts/run_container_lib.sh` (shared functions: `validate_common_env`, `print_banner`, `make_dpdk_dir`, `run_ano_container`, `run_dds_container`)
   - Create `tests/scripts/run_container.sh` (unified `transport:role` dispatcher)
   - Replace 8 individual scripts with one-line `exec` wrappers (backward-compatible)
   - Update `MultiProcessOrchestrator` in `test_utils.py` to invoke `run_container.sh <transport> <role>` directly
   - Update `CMakeLists.txt`: add `run_container_lib.sh` and `run_container.sh` to copy targets

4. **Implement Task 1 from `operators/connext/TASK.md`** (Priority: LOW):
   - Make `kAnoReaderPollInterval` YAML-configurable via `ano_poll_timeout_ms`
   - Currently hardcoded to 10ms in `connext_rx.cpp` — should be a YAML parameter
   - Requires rebuild of `connext_ops` target only

---

**Notes:**
- All implementation follows COPILOT.md guidelines
- SOLID principles applied throughout
- Clean code practices enforced (≤20 lines per function)
- Test-driven approach validated with working 1-to-many test
- Extensive documentation for maintainability
