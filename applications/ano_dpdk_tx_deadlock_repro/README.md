# ANO DPDK TX Deadlock Reproducer

Minimal reproducer for the DpdkMgr TX mempool/completion-polling deadlock documented
in [ANO_DPDK_TX_DEADLOCK_BUG.md](../../ANO_DPDK_TX_DEADLOCK_BUG.md).

## What it does

A single TX-only Holoscan operator paces UDP bursts onto a real NIC via the Advanced
Network DPDK backend. The YAML is sized so that

$$
I_{\max} = \left\lfloor \frac{\texttt{num\_bufs}}{\texttt{batch\_size}} \right\rfloor - 2 = 1
$$

i.e. only **one** in-flight burst is tolerated before `is_tx_burst_available()` returns
`false`. A producer-side sleep (`tx_pacing_us`) guarantees the worker's input ring
drains between submissions, so the mlx5 PMD's opportunistic `tx_free_thresh` reclaim
inside `rte_eth_tx_burst()` cannot rescue forward progress.

## Verdict protocol

The operator emits one of two terminal log lines and exits via `std::quick_exit`:

| Outcome                                | Log line                          | Exit code |
| -------------------------------------- | --------------------------------- | --------- |
| Deadlock reproduced (pre-fix)          | `*** DEADLOCK REPRODUCED ***`     | `0`       |
| Forward progress sustained (post-fix)  | `NO DEADLOCK OBSERVED`            | `1`       |
| Scheduler `max_duration_ms` exhausted  | "Run ended without a verdict..."  | `2`       |

This convention makes the reproducer scriptable: `! ./ano_dpdk_tx_deadlock_repro` passes
when the deadlock is reproduced, fails when the fix is in place.

## Configuration

Edit [ano_dpdk_tx_deadlock_repro.yaml](ano_dpdk_tx_deadlock_repro.yaml) and replace:

- `interfaces[0].address` — BDF of a DPDK-bound NIC port (e.g. `0000:b1:00.0`).
- `repro_tx.eth_dst_addr` — any destination MAC (broadcast `ff:ff:ff:ff:ff:ff` is fine; no peer required).

The reproducer needs only a real TX-capable NIC; nothing has to receive the packets.

## Build

```bash
./holohub build ano_dpdk_tx_deadlock_repro --language=cpp
```

## Run

```bash
./holohub run ano_dpdk_tx_deadlock_repro --docker-opts "-u root --privileged" --language cpp
```

Or, inside the container, directly:

```bash
./build/ano_dpdk_tx_deadlock_repro/applications/ano_dpdk_tx_deadlock_repro/cpp/ano_dpdk_tx_deadlock_repro \
    ano_dpdk_tx_deadlock_repro.yaml
echo "exit=$?"
```

## Expected behavior

**Before the fix (current `main`):**

```
... is_tx_burst_available=false consecutive=10000 | sent_bursts=1 sent_packets=1024
... is_tx_burst_available=false consecutive=20000 | sent_bursts=1 sent_packets=1024
...
*** DEADLOCK REPRODUCED *** is_tx_burst_available() has returned false 200000 times
in a row. sent_bursts=1 sent_packets=1024 (target=200000). ...
exit=0
```

(`sent_bursts` will typically be 1 or 2 depending on whether the PMD's first
opportunistic completion fires.)

**After the §5.1 fix is applied to `tx_core_worker`:**

```
NO DEADLOCK OBSERVED: sent_packets=200000 bursts=196 (target=200000). ...
exit=1
```

## Why the YAML is sized so tight

The bench's default configs use `num_bufs ≫ batch_size`, which puts $I_{\max}$ high
enough that steady-state throughput and the PMD's opportunistic reclaim almost always
keep the mempool above the gate threshold. That hides the structural defect. This
reproducer deliberately leaves no headroom so the bug is triggered on the very first
moment the worker's ring goes idle.
