# Connext Operators and Library

The `operators/connext` directory hosts two complementary components that enable RTI Connext DDS integrations on the
Holoscan platform:

- **`connext_lib`** – A reusable C++ support library that bundles DDS resource management, and ANO in the future.
- **`connext_ops`** – C++ Python transmit/receive operators (`ConnextTxOp` and `ConnextRxOp`) 

## Prerequisites

Before building any Connext component, make sure the following prerequisites are satisfied:
- RTI Connext DDS 7.3.0 SDK installed and `NDDSHOME` pointing at the SDK root (for example `/opt/rti/rti_connext_dds-7.3.0`).
- Valid RTI license file with `RTI_LICENSE_FILE` exporting the absolute path.
- CUDA runtime libraries by exporting `LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH`.

## Building

Both components are wired into the Holohub CMake build:

Ensure your environment exposes the RTI license file, NDDSHOME from the RTI Connext Debian packages, and CUDA runtime
libraries, for example:

```sh
export NDDSHOME=/opt/rti/rti_connext_dds-7.3.0
export RTI_LICENSE_FILE=/path/to/rti_license.dat
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```

> The Debian installers published by RTI place the SDK under `/opt/rti/rti_connext_dds-7.3.0`. Point `NDDSHOME` at
> that directory and ensure the license file is reachable before building the DDS helpers—CMake will fail early if the
> SDK is missing.

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