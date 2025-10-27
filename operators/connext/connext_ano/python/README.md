# Connext Holoscan Operators (ANO)

This package bundles the Connext-based Holoscan operators that orchestrate discovery and payload exchange.
Import the modules through the `connext_ano` namespace, e.g. `from connext_ano.connext_op_rx import ConnextOpRx`.

## Prerequisites
- Python 3.10 or higher
- RTI Connext DDS 7.3.0 Python package: `pip install rti.connext==7.3.0`
- Development dependencies for testing: `pip install pytest`
- The companion `connext_lib` package available on `PYTHONPATH` (installed automatically when you follow the steps below).

## Running the Tests

Install the package in editable mode (this also exposes the shared library helpers) and invoke pytest:

```sh
pip install -e ../connext_lib/python
pip install -e .[dev]
pytest
```

## RTI License Setup

This project requires a valid RTI Connext DDS license compatible with version **7.3.0**.

Set the environment variable `RTI_LICENSE_FILE` to the path of your RTI license file:

```sh
export RTI_LICENSE_FILE=/path/to/rti_license.dat
```

## Additional Information

- Operator sources live in [`src/connext_ano`](src/connext_ano).
- Tests are located in [`tests`](tests).
- Use `ctest` from the build directory when building Holohub with `BUILD_TESTING=ON` to exercise this test suite alongside the rest of the project.
