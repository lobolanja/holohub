# Connext ANO Skeleton

This directory holds the barebones pieces for Advanced Networking Operators built on top of
`holohub.connext_lib`. The goal is to keep things minimal so we can iterate on the transport logic step by step.

## Contents
- `connext_op_tx/`: simple Holoscan operator that writes an input buffer to shared memory and invokes the shared
  `ConnextTx` helper.
- `connext_op_rx/`: matching operator that waits for a buffer and emits it to downstream stages.
- `python/`: lightweight `holohub.connext_ano` namespace package plus smoke tests.

## How to evolve
1. Flesh out the control plane (discovery, capability exchange) inside the Tx/Rx operators.
2. Add the ANO fast path (RDMA/DPDK) to `holohub.connext_lib` once the plumbing is ready.
3. Expand the tests and metadata/docker integration as new behaviour is introduced.

For now, the bundle stays simple—use it as the starting point for the full Connext ANO operator family.
