# Connext Operators and Library

The `operators/connext` directory hosts two complementary components that enable RTI Connext DDS integrations on the
Holoscan platform:

- **`connext_lib`** – A reusable Python support library that bundles DDS resource management, shared-memory payload IO,
  and high-level send/receive helpers. During a CMake build (with `HOLOHUB_BUILD_PYTHON=ON`), this package is copied into
  the Holohub Python namespace so other operators can simply `import holohub.connext_lib` or install the library
  standalone via its `pyproject.toml`.
- **`connext_ano`** – Python transmit/receive operators (`ConnextTxOp` and `ConnextRxOp`) built on the support library to
  advertise and consume buffers over DDS.
[//]: # (TODO: rewrite connext_ano description when it is done)

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

### C++ helper library

```sh
cmake -S operators/connext -B build  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --target connext_lib connext_lib_tests
ctest --test-dir build -R connext_lib -V
```

- `connext_lib` emits a static archive plus headers/metadata so future native operators can link against a stable
  interface.
- `connext_lib_tests` runs the RTI unit-test framework so both the placeholder version helper and the DDS roundtrip helper
  execute inside a single binary.
- Running `ctest` exercises the placeholder test and integrates it with the broader Holohub test suite.

### Python packages

```sh
cmake -S operators/connext -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib_tests connext_lib_python connext_ano_python
ctest --test-dir build -R connext -V
```

- `connext_lib_python` mirrors the support library into `build/python/lib/holohub/connext_lib`, including its pytest
  suite and `pyproject.toml`.
- `connext_ano_python` stages the Holoscan operators under `build/python/lib/holohub/connext_ano`.
- Building `connext_lib_tests` ensures the shared helper remains linkable when the Python packages depend on it.

When `BUILD_TESTING` is enabled, `ctest` registers the C++ smoke tests plus both pytest collections so they can execute
alongside the rest of the Holohub tests.
