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

## Data Path Overview
- Transmit/receive operators negotiate transport as part of their startup handshake.
- ANO is used for the fast path whenever both peers advertise ANO capability and compatible hardware.
- If discovery shows endpoints limited to DDS—or ANO activation otherwise fails—the operators fall back to DDS for payload exchange.
- DDS discovery stays active regardless to handle metadata exchange and facilitate interop with DDS-only participants.

## Running Tests
- Prerequisites:
  - Activate or reference the project venv (`<HOLOHUB_ROOT>/.venv`); pip packages install into it.
  - Install CuPy with CUDA 12 wheels: `.venv/bin/python -m pip install cupy-cuda12x`.
  - Install the Holoscan Python package: `.venv/bin/python -m pip install holoscan`.
  - Provide the CUDA runtime libraries via pip: `.venv/bin/python -m pip install nvidia-cuda-runtime-cu12`.
  - Export the CUDA runtime on the library path for the session:
    `export LD_LIBRARY_PATH=<HOLOHUB_ROOT>/.venv/lib/python3.10/site-packages/nvidia/cuda_runtime/lib:$LD_LIBRARY_PATH`.
- Run the operator tests from `operators/connext/connext_ano/python` using `.venv/bin/pytest -q`.
