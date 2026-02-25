# Connext Operator — Task Backlog

This file tracks pending implementation tasks for the `operators/connext` subsystem.
Each task is self-contained and includes all context needed to implement it independently.

---

## Task 1: Expose ANO Reader Poll Timeout as YAML-Configurable Parameter

**Status:** `not-started`

### Background

During many-to-one testing (2 TX publishers → 1 RX subscriber via DPDK/ANO), the RX process
crashed with:

```
[critical] [adv_network_dpdk_mgr.cpp:1622] Running out of RX meta buffers due to high rates.
Either increase your number of metadata buffers (current: 256) with `rx_meta_buffers`
(will increase memory usage) or increase your `batch_size` for port 0 queue 0 (will increase latency)
terminate called without an active exception
```

Root cause analysis identified a timing misalignment between three independently configured values:

```
PeriodicCondition(message_period_ms)          ← YAML: demo.message_period_ms
  → ConnextRxOp::compute()
      → ConnextANOReader::readSamples()
          → ConnextRx::receive(kAnoReaderPollInterval)   ← HARDCODED in connext_rx.cpp
              → ANOPayloadReader::readNext(timeout)
                  loop { receive(); sleep(0.5ms); }
```

`kAnoReaderPollInterval` is hardcoded at 100ms in `connext_rx.cpp`:

```cpp
// operators/connext/connext_ops/cpp/src/connext_rx.cpp, line 133
constexpr std::chrono::milliseconds kAnoReaderPollInterval{100};
ano_reader_ = std::make_unique<connext_lib::ConnextANOReader>(
    ano_config_, dds_config_, kAnoReaderPollInterval);
```

**The conflict:** If `PeriodicCondition` is set to call `compute()` every 10ms, but
`readNext()` blocks for up to 100ms on each call, the Holoscan scheduler cannot fire at 10ms
intervals — it is stalled inside `readNext()`. The effective poll rate degrades back toward
100ms regardless of `message_period_ms`.

### Partial Fix Already Applied (YAML-only, no rebuild needed)

Changes in `applications/connext/connext_app_cpp/tests/config/test_ano_rx_many_to_many_sub1.yaml`:

- `rx_meta_buffers: 2048` — raised DPDK meta buffer pool from default 256 (8× more slots)
- `batch_size: 256` — reduced from 10240 so bursts don't accumulate as long between drains
- `message_period_ms: 10` — Holoscan calls `compute()` 10× more often, draining queue faster

The remaining gap: `kAnoReaderPollInterval` is still 100ms, so the 10ms `PeriodicCondition`
benefit is partially undermined — each `compute()` call can still block for up to 100ms.

### Goal

Add a new YAML parameter `ano_poll_timeout_ms` to `ConnextRxOp` that controls the timeout
passed to `ConnextANOReader`, allowing it to be aligned with `demo.message_period_ms`
without recompiling.

**Desired YAML usage:**
```yaml
connext_rx:
  ano_poll_timeout_ms: 10   # Must be ≤ demo.message_period_ms to avoid scheduler stalls
```

Default value: `100` — preserves existing behaviour for all configs that omit it.

### Files to Change

#### 1. `operators/connext/connext_ops/cpp/include/connext_ops/connext_rx.hpp`

Add a new private `Parameter` member after `ano_queue_id_` (~line 53):

```cpp
Parameter<uint32_t> ano_poll_timeout_ms_;
```

#### 2. `operators/connext/connext_ops/cpp/src/connext_rx.cpp`

In `setup()`, register the parameter after the `ano_queue_id_` block (~line 95):

```cpp
spec.param(ano_poll_timeout_ms_,
           "ano_poll_timeout_ms",
           "ANO Reader Poll Timeout (ms)",
           "Timeout passed to ConnextANOReader on each compute() call. "
           "Should be ≤ demo.message_period_ms to avoid stalling the Holoscan scheduler. "
           "Default: 100ms (preserves legacy behaviour).",
           static_cast<uint32_t>(100));
```

In `start()`, replace the hardcoded constant (~line 133):

```cpp
// Before:
constexpr std::chrono::milliseconds kAnoReaderPollInterval{100};

// After:
const std::chrono::milliseconds kAnoReaderPollInterval{ano_poll_timeout_ms_.get()};
```

No other C++ files need changing — the value flows naturally through the existing call chain.

#### 3. RX YAML configs (optional, for self-documentation)

Add `ano_poll_timeout_ms` explicitly, matching `demo.message_period_ms`:

```yaml
connext_rx:
  ano_poll_timeout_ms: 10  # Match demo.message_period_ms to avoid scheduler stall
```

Configs to update:
- `applications/connext/connext_app_cpp/tests/config/test_ano_rx_many_to_many_sub1.yaml`
- `applications/connext/connext_app_cpp/tests/config/test_ano_rx_many_to_many_sub2.yaml`
- `applications/connext/connext_app_cpp/tests/config/test_ano_rx_many_to_many_sub3.yaml`
- `applications/connext/connext_app_cpp/tests/config/test_ano_rx_1_to_many_sub1.yaml`
- `applications/connext/connext_app_cpp/tests/config/test_ano_rx_1_to_many_sub2.yaml`
- `applications/connext/connext_app_cpp/tests/config/test_ano_rx_1_to_many_sub3.yaml`
- `applications/connext/connext_app_cpp/tests/config/test_ano_rx.yaml`
- `applications/connext/connext_app_cpp/connext_receiver.yaml`

### Call Chain Reference

```
YAML: connext_rx.ano_poll_timeout_ms = 10
  │
  ▼
ConnextRxOp::setup()                          [connext_rx.cpp]
  spec.param(ano_poll_timeout_ms_, ...)
  │
  ▼
ConnextRxOp::start()                          [connext_rx.cpp]
  kAnoReaderPollInterval = milliseconds(ano_poll_timeout_ms_.get())
  ano_reader_ = make_unique<ConnextANOReader>(ano_config_, dds_config_, kAnoReaderPollInterval)
  │
  ▼
ConnextANOReader::ConnextANOReader(...)        [connext_readers.cpp]
  poll_interval_ms_ = kAnoReaderPollInterval
  rx_ = make_unique<ConnextRx>(receiver_manager, payload_reader)
  │
  ▼
ConnextANOReader::readSamples()               [connext_readers.cpp]
  rx_->receive(poll_interval_ms_)
  │
  ▼
ConnextRx::receive(timeout=10ms)              [connext_comm.cpp:42]
  payload_reader_->readNext(data_ptr, size, timeout)
  │
  ▼
ANOPayloadReader::readNext(timeout=10ms)      [payload_transport_ano.cpp:173]
  deadline = now() + 10ms
  while (now() < deadline) {
    receiver_->receive();    ← GpuDirectNetworkReceiver polls DPDK queue
    if no data: sleep(0.5ms)
  }
```

### Design Constraints

1. **Do not change `ConnextANOReader` or `ConnextRx` signatures** — `poll_interval_ms` is
   already a constructor argument. Only the call site in `ConnextRxOp::start()` changes.
2. **Default must remain 100ms** — all existing YAML configs that omit `ano_poll_timeout_ms`
   must continue to work identically.
3. **Document the ≤ constraint** — if `ano_poll_timeout_ms` > `demo.message_period_ms`,
   the scheduler stalls and the effective poll rate reverts to the timeout value.

### Build

Only `connext_ops` needs rebuilding (not `connext_lib` or `connext_ano_lib`):

```bash
cmake --build build-connext_app_cpp --target connext_ops
cmake --build build-connext_app_cpp --target connext_app_tests
```

### Verification

```bash
cd build-connext_app_cpp/applications/connext/connext_app_cpp/cpp/tests
python3 test_ano_many_to_many_e2e.py --timeout 90
```

Expected: no `Running out of RX meta buffers` crash, per-publisher reception ≥ 80%.

---
