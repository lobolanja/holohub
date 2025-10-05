# Connext Python Support Library

The Connext support library bundles reusable Python helpers for building RTI Connext DDS–enabled Holoscan operators. It
exposes utilities for DDS resource management, shared-memory payload IO, and high-level send/receive helpers that other
operators can import through the `holohub.connext_lib` package once the repository is configured with CMake.

## Layout
- `python/src/connext_lib`: Source package containing `comm`, `payload_io`, and `system_setup` modules.
- `python/tests`: Pytest-based unit tests covering the helpers.
- `python/pyproject.toml`: Metadata for the standalone Python package.

## Build Integration
The CMake integration copies the Python package into the Holohub namespace during the build so that any operator can
`import holohub.connext_lib` or `from holohub.connext_lib.system_setup import ...`.

### Building with CMake
```sh
cmake -S . -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib_python
```

### Running Tests via CTest
After configuring with `BUILD_TESTING=ON`, the Connext unit tests are registered with CTest. Run them from the build
tree:
```sh
ctest -C Release -R pytest.connext_lib -V
```
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
