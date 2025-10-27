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

```sh
cmake -S operators/connext -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib_python connext_ano_python
ctest --test-dir build -R connext -V
```

The `connext_lib_python` target mirrors the support library into
`build/python/lib/holohub/connext_lib`, including its pytest suite and `pyproject.toml`. The
`connext_ano_python` target stages the Holoscan operators under `build/python/lib/holohub/connext_ano`.
When `BUILD_TESTING` is enabled, `ctest` registers both pytest collections so they can execute alongside the rest of the
Holohub tests.
