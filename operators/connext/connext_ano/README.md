# Connext ANO Holoscan Operators

Work-in-progress transmit and receive operators that coordinate DDS discovery with an optional ANO fast path.
Whenever a behaviour still needs clarification you will find a `[TODO-JUANCA]` marker in the code so it can be
refined iteratively.

## Modules
- `common.py` – shared configuration dataclasses and simple transport state helper.
- `connext_op_tx.py` – transmit-side operator built on top of `holohub.connext_lib.ConnextTx`.
- `connext_op_rx.py` – receive-side operator using `holohub.connext_lib.ConnextRx`.
- `tests/` – minimal sanity checks to ensure the constructors keep working while we iterate.

Refer to `operators/connext/dds/` for the simpler DDS-only operators that inspired this skeleton.
