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
`import holohub.connext_lib` or `from holohub.connext_lib.system_setup import ...`. Tests and metadata are also copied to
aid downstream development.

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

Ensure the `RTI_LICENSE_FILE` environment variable points to a valid RTI Connext DDS 7.3.0 license file before running
the tests or examples.
