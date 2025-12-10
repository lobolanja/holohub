# Connext Operators and Library

The `operators/connext` directory hosts two complementary components that enable RTI Connext DDS integrations on the
Holoscan platform:

- **`connext_lib`** – A reusable Python support library that bundles DDS resource management, shared-memory payload IO,
  and high-level send/receive helpers. During a CMake build (with `HOLOHUB_BUILD_PYTHON=ON`), this package is copied into
  the Holohub Python namespace so other operators can simply `import holohub.connext_lib` or install the library
  standalone via its `pyproject.toml`.
- **`connext_ano`** – Python transmit/receive operators (`ConnextTxOp` and `ConnextRxOp`) built on the support library.
  They coordinate DDS discovery and negotiate an optional ANO fast path when both peers support shared-memory transport.
- **`connext_ops`** – C++ Python transmit/receive operators (`ConnextTxOp` and `ConnextRxOp`) 

## Prerequisites

Before building any Connext component, make sure the following prerequisites are satisfied:
- RTI Connext DDS 7.3.0 SDK installed and `NDDSHOME` pointing at the SDK root (for example `/opt/rti/rti_connext_dds-7.3.0`).
- RTI Connext Python bindings staged locally: `pip install rti.connext==7.3.0`.
- Valid RTI license file with `RTI_LICENSE_FILE` exporting the absolute path.
- CUDA runtime libraries available either via CuPy (`pip install cupy` or the CUDA-specific wheel such as `cupy-cuda12x`) or by exporting `LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH`.

These environment variables must be present when invoking CMake or running the tests. Keep the CUDA configuration
consistent (all CuPy wheels and toolkits should target the same CUDA major/minor version).

## Building

Both components are wired into the Holohub CMake build:

Ensure your environment exposes the RTI license file, NDDSHOME from the RTI Connext Debian packages, and CUDA runtime
libraries, for example:

```sh
export NDDSHOME=/opt/rti/rti_connext_dds-7.3.0
export RTI_LICENSE_FILE=/path/to/rti_license.dat
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```

> The Debian installers published by RTI place the SDK under `/opt/rti/rti_connext_dds-<version>`. Point `NDDSHOME` at
> that directory and ensure the license file is reachable before building the DDS helpers—CMake will fail early if the
> SDK is missing. If you only need the Python packaging, disable the native build via `-DCONNEXT_LIB_BUILD_CPP=OFF`.

### RTI license reminder

All Connext components require a valid RTI license. If you obtained the SDK through your organization, ask your RTI
administrator for the license file. Otherwise, visit the RTI Customer Portal to request an evaluation. Export
`RTI_LICENSE_FILE` before launching builds, tests, or sample applications.

### C++ helper library and operators

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --target connext_lib connext_lib_cpp_tests connext_ops connext_ops_cpp_tests
ctest --test-dir build -R connext -V
```

- `connext_lib` emits a static archive plus headers/metadata so future native operators can link against a stable
  interface.
- `connext_lib_cpp_tests` is a convenience target that builds every C++ unit-test binary
  (`connext_lib_dds_tests`, `connext_lib_config_tests`, and `connext_lib_payload_transport_tests`) so they can be run
  together or individually via `ctest`.
- Running `ctest` exercises the placeholder test and integrates it with the broader Holohub test suite.

### Python packages

```sh
cmake -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib_python connext_ano_python
```

- `connext_lib_python` mirrors the support library into `build/python/lib/holohub/connext_lib`, including its pytest
  suite and `pyproject.toml`.
- `connext_ano_python` stages the Holoscan operators under `build/python/lib/holohub/connext_ano`.
- Building the C++ tests ensures the shared helper remains linkable when the Python packages depend on it.

When `BUILD_TESTING` is enabled, `ctest` registers the C++ smoke tests plus both pytest collections so they can execute
alongside the rest of the Holohub tests.
