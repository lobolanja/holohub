# Connext Operators and Library

The `operators/connext` directory hosts two complementary components that enable RTI Connext DDS integrations on the
Holoscan platform:

- **`connext_lib`** – A reusable Python support library that bundles DDS resource management, shared-memory payload IO,
  and high-level send/receive helpers. During a CMake build (with `HOLOHUB_BUILD_PYTHON=ON`), this package is copied into
  the Holohub Python namespace so other operators can simply `import holohub.connext_lib` or install the library
  standalone via its `pyproject.toml`.
- **`connext_ano`** – Python transmit/receive operators (`ConnextTxOp` and `ConnextRxOp`) built on the support library to
  advertise and consume buffers over DDS.

## Building

Both components are wired into the Holohub CMake build:

```sh
cmake -S . -B build -DHOLOHUB_BUILD_PYTHON=ON
cmake --build build --target connext_lib_python connext_ano
```

The `connext_lib_python` target populates `build/python/lib/holohub/connext_lib` with the library sources, metadata, and
unit tests. The `connext_ano` target compiles the Connext operator.

## Developing the Library

Within `connext_lib/python` you can install the package in editable mode and run its unit tests:

```sh
pip install -e operators/connext/connext_lib/python[dev]
pytest operators/connext/connext_lib/python/tests
```

Make sure the RTI Connext DDS Python bindings (`rti.connext==7.3.0`) and a valid `RTI_LICENSE_FILE` are available in your
environment before running the tests or launching applications that rely on the operator.
