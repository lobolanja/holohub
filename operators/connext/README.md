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

Ensure your environment exposes the RTI license file and CUDA runtime libraries, for example:

```sh
export RTI_LICENSE_FILE=/path/to/rti_license.dat
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```

### C++ helper library

```sh
cmake -S operators/connext -B build -DBUILD_TESTING=ON
cmake --build build --target connext_lib connext_lib_version_test
ctest --test-dir build -R connext_lib_version_test -V
```

- `connext_lib` emits a static archive plus headers/metadata so future native operators can link against a stable
  interface.
- `connext_lib_version_test` builds a tiny executable that calls `connext_lib::version()` to ensure the library is linkable.
- Running `ctest` exercises the placeholder test and integrates it with the broader Holohub test suite.

### Python packages

```sh
cmake -S operators/connext -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib_version_test connext_lib_python connext_ano_python
ctest --test-dir build -R connext -V
```

- `connext_lib_python` mirrors the support library into `build/python/lib/holohub/connext_lib`, including its pytest
  suite and `pyproject.toml`.
- `connext_ano_python` stages the Holoscan operators under `build/python/lib/holohub/connext_ano`.
- Building `connext_lib_version_test` ensures the shared C++ helper remains linkable when the Python packages depend on it.

When `BUILD_TESTING` is enabled, `ctest` registers the C++ smoke test plus both pytest collections so they can execute
alongside the rest of the Holohub tests.
