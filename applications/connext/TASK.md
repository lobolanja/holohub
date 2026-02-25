# Connext Application — Task Backlog

This file tracks pending implementation tasks for the `applications/connext` subsystem.
Each task is self-contained and includes all context needed to implement it independently.

---

## Task 1: Dual-Transport Support in `ConnextTxOp` and `ConnextRxOp` (ANO + DDS Simultaneously)

**Status:** `completed`

---

### Error Encountered

**Initial error (mutual-exclusion guard):**

```
[error] [gxf_wrapper.cpp:63] Exception occurred when starting operator: 'connext_tx' -
ConnextTxOp currently supports only one transport at a time (DDS or ANO).
[warning] [entity_executor.cpp:539] Failed to start entity [connext_tx]
[warning] [greedy_scheduler.cpp:243] Error while executing entity 13 named 'connext_tx': GXF_FAILURE
[error] [entity_executor.cpp:641] Entity [connext_tx] must be in Started, Tick Pending, Ticking
or Idle stage before stopping. Current state is StartPending
[error] [main.cpp:43] Connext demo failed: ConnextTxOp currently supports only one transport at
a time (DDS or ANO).
```

**Follow-up error (ANO manager not initialized in dual-transport mode):**

```
[info] [connext_common.cpp:359] Connext sender configured. transport=dds+ano, ...
[info] [connext_tx.cpp:147] ConnextTxOp starting (dds_enabled=true, ano_enabled=true, ...)
connext_app: common.cpp:176: int holoscan::advanced_network::get_port_id(const string&):
Assertion `g_ano_mgr != nullptr && "Advanced Network Manager is not initialized"' failed.
Aborted (core dumped)
```

**Root cause of follow-up:** `ConnextDemoApp::compose()` gated `adv_net_init()` on
`!demo_config_.use_dds`. In dual-transport mode `use_dds == true`, so the condition
evaluated to `false` and the Advanced Network Manager was never initialized before
`ConnextANOWriter` tried to use it.

**Follow-up error (false-positive transport isolation failure):**

```
✗ RX1 (ANO) log contains DDS indicators: ['DDS', 'connext', 'RTI ']
  → DDS traffic may be leaking into the ANO subscriber
✗ RX2 (DDS) log contains ANO/DPDK indicators: ['hugepage']
  → DPDK traffic may be leaking into the DDS subscriber
✗ TEST FAILED — Transport isolation violation detected
```

Actual delivery: RX1=99.7%, RX2=100% — packet routing was correct. The failure was a
**false positive** in `check_transport_isolation()`. The keyword lists in
`test_mixed_transport_e2e.py` were too broad:

- `'DDS'`, `'connext'`, `'RTI '` appear in every container's startup logs because
  `ConnextRxOp` always logs its DDS configuration even in ANO-only mode.
- `'hugepage'` appears in the RX2 shell launcher script's environment check output,
  not from actual DPDK packet processing.

Fix: replace broad init-log terms with **runtime-only** indicators that only appear
when a transport is actually transmitting or receiving data.

---

### Background

The Mixed-Transport test topology (already planned in `tests/TEST_PLAN.md` and YAML configs
already present in `tests/config/test_mixed_tx.yaml`) requires a single TX container to
simultaneously write payloads over **both** ANO (GPU Direct / DPDK) and DDS transports per
`compute()` call:

```
TX (enable_ano: true, enable_dds: true)
  ├─── [ANO / DPDK] ──► RX1 (enable_ano: true,  enable_dds: false)
  └─── [DDS / RTI]  ──► RX2 (enable_ano: false, enable_dds: true)
```

Currently, both `ConnextTxOp` and `ConnextRxOp` enforce a mutual-exclusion guard at `start()`:

```cpp
// operators/connext/connext_ops/cpp/src/connext_tx.cpp  (line ~131)
} else if (dds_config_.enabled() && ano_config_.enabled()) {
    throw std::runtime_error(
        "ConnextTxOp currently supports only one transport at a time (DDS or ANO).");
}

// operators/connext/connext_ops/cpp/src/connext_rx.cpp  (line ~121)
} else if (dds_config_.enabled() && ano_config_.enabled()) {
    throw std::runtime_error(
        "ConnextRxOp currently supports only one transport at a time (DDS or ANO).");
}
```

Removing this restriction and correctly handling both transports in `compute()` is the
minimal change needed to make the mixed-transport scenario work.

---

### Architecture Analysis

#### Current State (single-transport)

`ConnextTxOp` holds two `unique_ptr` writers and only one is ever non-null:

```cpp
// connext_tx.hpp
std::unique_ptr<connext_lib::ConnextDDSWriter> dds_writer_;
std::unique_ptr<connext_lib::ConnextANOWriter> ano_writer_;
```

`ConnextTxOp::start()` initialises exactly one of them:

```cpp
if (dds_config_.enabled()) {
    dds_writer_ = std::make_unique<...>(dds_config_, ...);
} else if (ano_config_.enabled()) {
    ano_writer_ = std::make_unique<...>(ano_config_, dds_config_, ...);
}
```

`ConnextTxOp::compute()` dispatches to whichever is non-null:

```cpp
if (ano_config_.enabled()) {
    // Requires GPU tensor (is_gpu_tensor check)
    ano_writer_->broadcast(buffer);
} else if (dds_config_.enabled()) {
    // Accepts both; GPU tensors are auto-copied to CPU
    dds_writer_->broadcast(buffer);
}
```

`ConnextRxOp` mirrors the pattern on the receive side.

#### Target State (dual-transport TX)

For TX, when both are enabled:
1. **Initialise both writers** at `start()`.
2. In `compute()`, extract the payload tensor once, then dispatch to **both** writers.
   - ANO path: must receive a GPU tensor — verify and pass as-is.
   - DDS path: accepts either; if the tensor is on GPU, perform the host-copy internally.
3. The `PayloadSourceOp` `use_gpu_memory` parameter must be `true` when ANO is active
   (upstream already sets this — see `connext_common.cpp::configure_tx_operators()`).

For RX, dual-transport reception is a less common pattern (a single `compute()` that
tries both transports and emits whichever arrives first). The test topology does **not**
require dual-transport RX — each RX container uses exactly one transport. Implementing
dual-transport RX is therefore **out of scope for this task**.

---

### Memory-Type Constraint in Dual-Transport TX

The key tension:

| Transport | Required memory type         |
|-----------|------------------------------|
| ANO       | GPU (MemoryStorageType::kDevice) — zero-copy RDMA |
| DDS       | CPU (MemoryStorageType::kSystem) — DDS middleware |

When both are enabled in TX, the upstream `PayloadSourceOp` **must** produce a GPU tensor
(to satisfy ANO). The DDS path must then internally copy from GPU to CPU.

This is already handled inside `ConnextTxOp::compute()` for the DDS-only path:

```cpp
// existing GPU→CPU copy in DDS branch
std::vector<std::uint8_t> cpu_data(payload_size);
cudaMemcpy(cpu_data.data(), data, payload_size, cudaMemcpyDeviceToHost);
connext_lib::MemoryBufferView buffer{cpu_data.data(), payload_size, false};
dds_writer_->broadcast(buffer);
```

In dual-transport mode the ANO path runs first (using the original GPU pointer) and then
the DDS copy runs second — both within the same `compute()` call. The GPU allocation
remains valid throughout because the tensor is still live on the stack.

---

### Pseudo-Code

#### `ConnextTxOp::start()` — remove mutual exclusion, initialise both writers

```
function start():
    Operator::start()
    refresh_configs()

    if NOT dds_config.enabled AND NOT ano_config.enabled:
        throw "ConnextTxOp requires at least one transport (DDS or ANO)"

    // No longer mutually exclusive — both can be true
    if dds_config.enabled:
        dds_writer = make_unique<ConnextDDSWriter>(dds_config, ano_max_payload)

    if ano_config.enabled:
        ano_writer = make_unique<ConnextANOWriter>(ano_config, dds_config, poll_interval_ms)

    log("ConnextTxOp starting (dds={}, ano={})", dds_config.enabled, ano_config.enabled)
```

#### `ConnextTxOp::compute()` — dispatch to all active transports

```
function compute(input, output, context):
    entity = input.receive("input")
    if not entity: return

    tensor = entity.get<Tensor>("payload")
    if not tensor: return

    data        = tensor.data()
    payload_size = tensor.nbytes()
    is_gpu      = (tensor.device.device_type == kDLCUDA)

    // --- ANO path (requires GPU memory) ---
    if ano_config.enabled AND ano_writer != null:
        if NOT is_gpu:
            throw "ANO transport requires GPU memory"
        ano_writer.broadcast({data, payload_size, is_device=true})

    // --- DDS path (accepts GPU or CPU; copies if necessary) ---
    if dds_config.enabled AND dds_writer != null:
        if is_gpu:
            cpu_data = vector<uint8_t>(payload_size)
            cudaMemcpy(cpu_data.data(), data, payload_size, DeviceToHost)
            dds_writer.broadcast({cpu_data.data(), payload_size, is_device=false})
        else:
            dds_writer.broadcast({data, payload_size, is_device=false})
```

#### `ConnextTxOp::stop()` — unchanged (both unique_ptrs already reset)

```
function stop():
    dds_writer.reset()   // no-op if null
    ano_writer.reset()   // no-op if null
    Operator::stop()
```

---

### Implementation Steps

#### Step 1 ✅ — Remove mutual-exclusion guard in `ConnextTxOp::start()`

**File:** `operators/connext/connext_ops/cpp/src/connext_tx.cpp`

Locate the guard block (~line 131):

```cpp
} else if (dds_config_.enabled() && ano_config_.enabled()) {
    throw std::runtime_error(
        "ConnextTxOp currently supports only one transport at a time (DDS or ANO).");
}
```

Replace the entire `if / else if / else if` initialization block with independent `if`
checks so both writers can be constructed simultaneously:

```cpp
void ConnextTxOp::start() {
  Operator::start();
  refresh_configs();

  if (!dds_config_.enabled() && !ano_config_.enabled()) {
    throw std::runtime_error(
        "ConnextTxOp requires at least one transport to be enabled (DDS or ANO).");
  }

  if (dds_config_.enabled()) {
    dds_writer_ = std::make_unique<connext_lib::ConnextDDSWriter>(
        dds_config_, static_cast<int>(ano_max_payload_.get()));
  }

  if (ano_config_.enabled()) {
    constexpr std::chrono::milliseconds kAnoWriterPollInterval{100};
    ano_writer_ = std::make_unique<connext_lib::ConnextANOWriter>(
        ano_config_, dds_config_, kAnoWriterPollInterval);
  }

  HOLOSCAN_LOG_INFO(
      "ConnextTxOp starting (dds_enabled={}, ano_enabled={}, channel={}, destination={})",
      dds_config_.enabled(),
      ano_config_.enabled(),
      ano_config_.channel_name(),
      destination_reference_.get());
}
```

#### Step 2 ✅ — Update `ConnextTxOp::compute()` for dual dispatch

**File:** `operators/connext/connext_ops/cpp/src/connext_tx.cpp`

Replace the existing `if (ano_config_.enabled()) { ... } else if (dds_config_.enabled()) { ... }`
dispatch with independent `if` blocks so both fire when both are enabled:

```cpp
void ConnextTxOp::compute(InputContext& input, OutputContext& output, ExecutionContext& context) {
  (void)output;
  (void)context;

  auto entity_expected = input.receive<gxf::Entity>("input");
  if (!entity_expected) { return; }

  auto tensor = entity_expected.value().get<Tensor>("payload");
  if (!tensor) { return; }

  auto* data = static_cast<std::uint8_t*>(tensor->data());
  if (!data) { return; }

  const auto payload_size = static_cast<std::size_t>(tensor->nbytes());
  const bool is_gpu_tensor = (tensor->device().device_type == kDLCUDA);

  // --- ANO path: requires GPU memory for zero-copy GPU Direct RDMA ---
  if (ano_config_.enabled() && ano_writer_) {
    if (!is_gpu_tensor) {
      throw std::runtime_error(
          "ConnextTxOp: ANO transport requires GPU memory (MemoryStorageType::kDevice), "
          "but received CPU tensor.");
    }
    connext_lib::MemoryBufferView ano_buffer{data, payload_size, true};
    ano_writer_->broadcast(ano_buffer);
  }

  // --- DDS path: accepts GPU or CPU; GPU tensors are auto-copied to CPU ---
  if (dds_config_.enabled() && dds_writer_) {
    if (is_gpu_tensor) {
      std::vector<std::uint8_t> cpu_data(payload_size);
      cudaError_t err = cudaMemcpy(
          cpu_data.data(), data, payload_size, cudaMemcpyDeviceToHost);
      if (err != cudaSuccess) {
        throw std::runtime_error(
            std::string("ConnextTxOp: Failed to copy GPU tensor to CPU for DDS: ") +
            cudaGetErrorString(err));
      }
      connext_lib::MemoryBufferView dds_buffer{cpu_data.data(), payload_size, false};
      dds_writer_->broadcast(dds_buffer);
    } else {
      connext_lib::MemoryBufferView dds_buffer{data, payload_size, false};
      dds_writer_->broadcast(dds_buffer);
    }
  }
}
```

**Key safety note:** `cpu_data` is declared inside the `if (is_gpu_tensor)` branch and
lives on the stack until after `dds_writer_->broadcast()` returns — the DDS writer copies
the payload bytes internally before returning, so there is no dangling pointer risk.

#### Step 3 ✅ — Verify `PayloadSourceOp` emits GPU tensors in dual-transport TX mode

**File:** `applications/connext/connext_app_cpp/cpp/connext_common.cpp`

In `ConnextDemoApp::configure_tx_operators()`, GPU memory selection is already correct:

```cpp
// Determine if GPU memory should be used (ANO mode requires GPU memory)
bool use_gpu = !demo_config_.use_dds;
```

This evaluates to `false` when `use_dds == true` (DDS-only) and `true` when ANO is
active. However, `demo_config_.use_dds` is loaded from `connext_tx.enable_dds` by
`load_demo_config()` — and in dual-transport mode both flags are `true`.

The `apply_transport_info` lambda reads only `enable_dds` (or `enable_ano`) and sets
`result.use_dds` accordingly. In dual-transport mode (`enable_dds: true, enable_ano: true`)
the value ends up as `use_dds = true`, which then sets `use_gpu = false` — **wrong** for
the ANO path.

Fix: change `use_gpu` logic to check whether ANO is explicitly enabled:

```cpp
// In configure_tx_operators():
// Use GPU memory whenever ANO is active (required for zero-copy RDMA).
// DDS can always accept GPU tensors — ConnextTxOp will copy them internally.
bool use_gpu = !demo_config_.use_dds;  // current (single-transport logic)

// Proposed fix:
bool ano_enabled = !demo_config_.use_dds;  // existing field captures "not DDS-only"
// Better: add a separate `use_ano` field to DemoAppConfig and parse it explicitly.
```

The cleanest fix is to add `bool use_ano = false;` to `DemoAppConfig` and populate it
independently in `apply_transport_info()`:

```cpp
// In DemoAppConfig (connext_common.hpp):
bool use_dds = false;
bool use_ano = false;   // NEW

// In apply_transport_info() (connext_common.cpp):
if (auto enable_dds_arg = from_config(enable_dds_key); enable_dds_arg.size() > 0) {
    result.use_dds = enable_dds_arg.as<bool>();
}
if (auto enable_ano_arg = from_config(section + ".enable_ano"); enable_ano_arg.size() > 0) {
    result.use_ano = enable_ano_arg.as<bool>();  // NEW
}

// In configure_tx_operators():
bool use_gpu = result.use_ano;  // GPU memory required whenever ANO is active
```

#### Step 4 ✅ — Update log messages to reflect dual-transport mode

**File:** `applications/connext/connext_app_cpp/cpp/connext_common.cpp`

The existing log line in `configure_tx_operators()` uses:

```cpp
config_.use_dds ? "dds" : "ano"
```

Update to reflect both transports when dual mode is active:

```cpp
std::string transport_label;
if (demo_config_.use_dds && demo_config_.use_ano) {
    transport_label = "dds+ano";
} else if (demo_config_.use_dds) {
    transport_label = "dds";
} else {
    transport_label = "ano";
}
```

#### Step 5 ✅ — Fix Advanced Network Manager initialization in dual-transport mode

**File:** `applications/connext/connext_app_cpp/cpp/connext_common.cpp`

`ConnextDemoApp::compose()` used `!demo_config_.use_dds` as a proxy for "ANO is active".
In dual-transport mode (`use_dds == true && use_ano == true`) this evaluates to `false`,
skipping `adv_net_init()` entirely. The ANO writer then asserts because
`g_ano_mgr == nullptr`.

Fix:

```cpp
// Before:
if (!demo_config_.use_dds) { ... adv_net_init() ... }

// After:
if (demo_config_.use_ano) { ... adv_net_init() ... }
```

#### Step 6 ✅ — Fix false-positive transport isolation check

**File:** `applications/connext/connext_app_cpp/tests/test_mixed_transport_e2e.py`

The `_DDS_INDICATORS` and `_ANO_INDICATORS` keyword lists matched terms that appear in
every container's startup/init logs regardless of which transport is active:

- `'DDS'`, `'connext'`, `'RTI '` — logged by `ConnextRxOp` setup even on ANO-only containers.
- `'hugepage'` — printed by the launcher shell script's environment check, not DPDK.

Fix: replace with **runtime-only** indicators that only appear when data is actually
being sent or received on that transport:

```python
# DDS runtime indicators (only appear during active DDS sample exchange)
_DDS_INDICATORS = [
    'DDS DataWriter',
    'DDS DataReader',
    'dds_topic',
    'domain_id',
]

# ANO/DPDK runtime indicators (only appear during active DPDK packet processing)
_ANO_INDICATORS = [
    'DPDK',
    'advanced_network',
    'ANO',
    'PCI:',
    'Port 0:',
]
```

Terms removed from `_DDS_INDICATORS`: `'DDS'`, `'connext'`, `'ConnextDDS'`, `'RTI '`.
Terms removed from `_ANO_INDICATORS`: `'dpdk'` (too broad — appears in scripts), `'hugepage'`.

#### Step 7 ✅ — Verify DDS well-known ports do not conflict with ANO UDP port

The TX log showed two distinct domain IDs in use:
- `domain_id: 42` — user-configured data-plane domain (from `test_mixed_tx.yaml`)
- `domain_id: 101` — internal resource manager IDL domain (`DdsIdlSenderResourcesManager`)

RTPS well-known port formula (RTPS spec §9.6.1):

```
PB = 7400  (port base)
DG = 250   (domain gain)

Metatraffic multicast : PB + DG × domain_id + 0
Metatraffic unicast   : PB + DG × domain_id + 10
User data multicast   : PB + DG × domain_id + 1
User data unicast     : PB + DG × domain_id + 11
```

Calculated port ranges:

| Domain ID | Source | Port range |
|-----------|--------|------------|
| 42 | User data plane | 17900 – 17930 |
| 101 | Internal IDL resource manager | 32650 – 32680 |

**ANO UDP port in the mixed test:** `7000`

**Verdict:** No conflict. ANO port 7000 is far below both DDS port ranges. The lowest
possible DDS port is 7400 (domain 0), so no valid domain ID can produce a DDS port at
or below 7000.

---

### Files to Change

| File | Change |
|------|--------|
| `operators/connext/connext_ops/cpp/src/connext_tx.cpp` | Step 1: remove mutual-exclusion guard in `start()`; Step 2: dual-dispatch in `compute()` |
| `applications/connext/connext_app_cpp/cpp/connext_common.hpp` | Step 3: add `bool use_ano` to `DemoAppConfig` |
| `applications/connext/connext_app_cpp/cpp/connext_common.cpp` | Step 3: populate `use_ano` from YAML; fix `use_gpu` logic; Step 4: update log messages; Step 5: fix `adv_net_init()` gate |
| `applications/connext/connext_app_cpp/tests/test_mixed_transport_e2e.py` | Step 6: tighten isolation keyword lists to runtime-only indicators |
| *(analysis only, no code change)* | Step 7: verify DDS well-known ports (domain 42 → 17900+, domain 101 → 32650+) do not conflict with ANO port 7000 |

No changes are needed to:
- `connext_lib/` — both `ConnextDDSWriter` and `ConnextANOWriter` are already independent
- `connext_tx.hpp` / `connext_rx.hpp` — both `unique_ptr` fields already exist
- YAML configs in `tests/config/` — `test_mixed_tx.yaml` already has `enable_dds: true` and
  `enable_ano: true`; the receiver YAMLs are already single-transport

---

### Call Chain Reference

```
YAML: connext_tx.enable_dds=true, connext_tx.enable_ano=true
  │
  ▼
ConnextTxOp::setup()          [connext_tx.cpp]
  spec.param(enable_dds_, ...)
  spec.param(enable_ano_, ...)
  │
  ▼
ConnextTxOp::start()          [connext_tx.cpp]  ← CHANGE: remove mutual exclusion
  dds_config_.set_enabled(true)
  ano_config_ = AnoConfig(..., enabled=true)
  dds_writer_ = make_unique<ConnextDDSWriter>(dds_config_, max_payload)
  ano_writer_ = make_unique<ConnextANOWriter>(ano_config_, dds_config_, 100ms)
  │
  ▼ (per Holoscan scheduler tick)
ConnextTxOp::compute()        [connext_tx.cpp]  ← CHANGE: dual dispatch
  tensor = receive("input")                       // GPU tensor from PayloadSourceOp
  │
  ├─ ano_writer_.broadcast({gpu_ptr, size, true})
  │    └─► ConnextANOWriter::broadcast()          [connext_writers.cpp]
  │            └─► ConnextTx::setBuffer() + broadcast()
  │                    └─► ANO / DPDK → RX1
  │
  └─ cudaMemcpy(cpu_buf, gpu_ptr, size, D2H)
     dds_writer_.broadcast({cpu_ptr, size, false})
          └─► ConnextDDSWriter::broadcast()       [connext_writers.cpp]
                  └─► DDS DataWriter → RX2
```

---

### Design Constraints

1. **Do not change `connext_lib/` signatures** — `ConnextDDSWriter` and `ConnextANOWriter`
   are already independent objects; only the call site in `ConnextTxOp` changes.
2. **ANO always requires GPU tensors** — the upstream `PayloadSourceOp` `use_gpu_memory`
   parameter must be `true` whenever ANO is active. Fix the `use_gpu` derivation logic
   so it checks `use_ano` rather than `!use_dds`.
3. **DDS GPU→CPU copy is safe within one `compute()` call** — `cpu_data` stays alive on the
   stack until `dds_writer_->broadcast()` returns; the DDS layer copies bytes before returning.
4. **Dual-transport RX is out of scope** — the mixed-transport test uses single-transport
   RX containers; each receiver uses exactly one transport.
5. **Default behaviour is unchanged** — configs with only one transport enabled continue
   to work identically; the guard only changed from "throw if both enabled" to "allow both".

---

### Build

Only `connext_ops` and the application binary need rebuilding:

```bash
cmake --build build-connext_app_cpp --target connext_ops
cmake --build build-connext_app_cpp --target connext_app_cpp
cmake --build build-connext_app_cpp --target connext_app_tests
```

`connext_lib` and `connext_ano_lib` do **not** need rebuilding — only call sites in
`connext_ops` change.

---

### Verification

Run the existing mixed-transport end-to-end test (already scaffolded in
`tests/test_mixed_transport_e2e.py`):

```bash
export TEST_TX_NIC_PCIE="0005:03:00.0"
export TEST_RX_NIC_PCIE="0005:03:00.1"
export RTI_LICENSE_FILE="./rti_license.dat"

cd applications/connext/connext_app_cpp/tests
python3 test_mixed_transport_e2e.py --timeout 90 --threshold 0.80
```

**Actual result (verified 2026-02-25):**
- Exit code `0` (PASS)
- RX1 (ANO): 337/338 messages — **99.7%** ✅
- RX2 (DDS): 338/338 messages — **100%** ✅
- Transport isolation check: **PASS** ✅ (after Step 6 keyword fix)
- No port conflict between DDS (domain 42 → ports 17900+; domain 101 → ports 32650+)
  and ANO (port 7000) ✅

**Regression check:**
Run the existing single-transport tests to verify no regressions:

```bash
python3 test_ano_e2e.py           # ANO-only (should still pass)
python3 test_dds_e2e.py           # DDS-only (should still pass)
python3 test_ano_1_to_many_e2e.py # ANO 1-to-many (should still pass)
```

---
