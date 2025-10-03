# Connext Operator Project Notes

## Current Goal
- Build a Connext-based Holoscan operator that leverages the Advanced Networking (ANO) library to transfer data GPU-to-GPU across NVIDIA Mellanox SmartNICs.
- Maintain DDS-like publish/subscribe semantics so the operator integrates naturally with existing DDS-based pipelines.

## Interim Implementation
- While NVIDIA SmartNIC hardware is unavailable, use the `abstract_payload_io` interface backed by a shared-memory payload I/O manager.
- Abstract design should allow a drop-in replacement with ANO-backed payload I/O once hardware access is available.
- Develop the `connext_lib` Python module to house reusable utilities (e.g., payload I/O helpers, DDS-style orchestration) so multiple operators can share the same code.
- Implement the `connext_ano` operator package with dedicated TX and RX operators that build on `connext_lib` abstractions.

## Implemented Components
- `operators/connext/connext_lib`: Python package exposing shared utilities such as `ConnextTx`/`ConnextRx` wrappers, shared-memory payload IO (`MemPayloadWriter`/`MemPayloadReader`), inter-process event bus helpers, and DDS-based resource managers (topic-driven and discovery-driven). Comes with metadata, CMake/pyproject wiring, and unit tests covering communication, payload IO, and discovery flows.
- `operators/connext/connext_ano`: Holoscan TX/RX operator pair that rely on `connext_lib` to advertise shared-memory buffers over DDS. Supports discovery vs. explicit topics, optional zero padding, multiple output modes, and includes metadata, README, Dockerfile, and Python tests to exercise the operator API.

## Running Tests in Holohub (Local Environment)
### Running tests installing the Connext library
- Configure a build with Python + testing enabled: `cmake -S . -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON`.
- Build just the Connext support library without compiling the rest of Holohub: `cmake --build build --target connext_lib_python`.
- Create and activate a dedicated Python virtual environment for testing: `python -m venv .venv && source .venv/bin/activate`.
- Ensure the RTI Python bindings, pytest, and license are available inside that venv: `pip install pytest rti.connext==7.3.0` and `export RTI_LICENSE_FILE=/path/to/rti_license.dat`.
- Run the library's test suite via CTest once the build completes: `cd build && ctest -R pytest` (passes through to the per-test `pytest` runners).

### Running tests directly from source
- When running directly from source, set `PYTHONPATH` to the package `src` layout (for example `export PYTHONPATH=$PWD/operators/connext/connext_lib/python/src:$PYTHONPATH`) and launch `python -m pytest operators/connext/connext_lib/python/tests`.
- Alternatively, after building, use the copied package in `build/python/lib` (`export PYTHONPATH=$PWD/build/python/lib:$PYTHONPATH`) and run the same `pytest` command. Both approaches integrate cleanly with the holohub container tooling.

## Container Workflow
- Build a tailored container from the provided Dockerfile: `./holohub build-container --docker-file operators/connext/connext_ano/Dockerfile --img holohub-connext-ano:latest`. Add `--base-img <ngc_image>` if you need to override the default Holoscan SDK base.
- Launch the container to get an interactive shell with all dependencies: `./holohub run-container --img holohub-connext-ano:latest` (pass `--` and your command to run non-interactively).
- Inside the container, mount or export `RTI_LICENSE_FILE` and invoke the Python tests as described above (for example `python -m pytest operators/connext/connext_lib/python/tests`).

## Next Steps / Open Items
- Exercise the shared-memory Tx/Rx flow inside a Holoscan graph and collect performance/timing data.
- Finalise the abstraction layer needed to swap `MemPayload*` helpers with ANO-backed implementations once hardware is accessible.
- Expand testing to cover end-to-end GPU buffer hand-off (unit + integration) and capture requirements for Mellanox SmartNIC validation.

_This document is intended to be updated as the project progresses._
