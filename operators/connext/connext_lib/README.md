# Connext Support Library

The Connext support library bundles reusable helpers for building RTI Connext DDS–enabled Holoscan operators. The Python
package exposes utilities for DDS resource management, shared-memory payload IO, and high-level send/receive helpers via
the `holohub.connext_lib` namespace, while the C++ component offers the same foundations to native operators.

## Prerequisites
- RTI Connext DDS 7.3.0 SDK available locally and `RTI_LICENSE_FILE` pointing at a valid license file.
- Python bindings staged via `pip install rti.connext==7.3.0` (the `pyproject` declares this dependency but pre-installing it avoids build-time network access).
- CUDA runtime libraries discoverable either through CuPy (`pip install cupy` or the CUDA-specific wheel such as
  `cupy-cuda12x`) or by exporting `LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH`.

## Layout
- `cpp/`: Native helper library and unit tests that expose DDS builders to C++ operators.
- `python/src/connext_lib`: Source package containing `comm`, `payload_io`, and `system_setup` modules.
- `python/tests`: Pytest-based unit tests covering the helpers.
- `python/pyproject.toml`: Metadata for the standalone Python package.

## Build Integration
The CMake integration copies the Python package into the Holohub namespace during the build so that any operator can
`import holohub.connext_lib` or `from holohub.connext_lib.system_setup import ...`.

### C++ helper library
```sh
cmake -B build -DCONNEXT_LIB_BUILD_CPP=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib connext_lib_cpp_tests
```

- `connext_lib` produces the static archive and headers under `build`.
- `connext_lib_cpp_tests` is an aggregate target that builds every native unit-test binary so they can be executed via
   CTest or directly.

### Python package
```sh
cmake -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib_python
```

### Running Tests via CTest
After configuring with `BUILD_TESTING=ON`, both the native and Python test suites are registered with CTest. From the
same build tree you can run:
- C++ tests:
   ```sh
   ctest --test-dir build -R connext_lib_ -V
   ```
- Python tests:
   ```sh
   ctest --test-dir build -R pytest.connext_lib -V
   ```
- All Connext library tests:
   ```sh
   ctest --test-dir build -R connext_lib -V
   ```
<!-- [TODO] JUANCA: change all testenames to start with connext_lib -->
Adjust `-C` for your active build type.

## Developing Locally
1. Create and activate a Python 3.10+ environment with access to `rti.connext==7.3.0`.
2. Install development dependencies:
   ```sh
   pip install -e operators/connext/connext_lib/python[dev]
   ```
3. Run the unit tests:
   ```sh
   pytest operators/connext/connext_lib/python/tests
   ```
   or rely on the staged package created by CMake:
   ```sh
   PYTHONPATH=build/python/lib/holohub pytest operators/connext/connext_lib/python/tests
   ```

Ensure the `RTI_LICENSE_FILE` environment variable points to a valid RTI Connext DDS 7.3.0 license file before running
the tests or examples.
