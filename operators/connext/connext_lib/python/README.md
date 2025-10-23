# Connext Holoscan Library

This project packages reusable DDS buffer advertisement and payload IO helpers for Holoscan. The modules ship inside the
`connext_lib` package and can be imported directly (for example, `from connext_lib.comm import ConnextTx`).

## Prerequisites
- Python 3.10 or higher
- RTI Connext DDS 7.3.0 Python package: `pip install rti.connext==7.3.0`
- Development dependencies for testing: `pip install pytest`

## Running the Tests

Install the package in editable mode and execute the tests with pytest:

```sh
pip install -e .[dev]
pytest
```

## RTI License Setup

This project requires a valid RTI Connext DDS license compatible with version **7.3.0**.

Set the environment variable `RTI_LICENSE_FILE` to the path of your RTI license file:

```sh
export RTI_LICENSE_FILE=/path/to/rti_license.dat
```

## Debugging in VS Code

1. Create a `.env` file in your workspace root with the license path:
   ```
   RTI_LICENSE_FILE=/path/to/rti_license.dat
   ```
2. Configure VS Code to load the `.env` file by setting `python.envFile` in `.vscode/settings.json`.
3. Launch the debugger normally; the environment variable will be picked up automatically.

## Additional Information

- Source code lives in [`src/connext_lib`](src/connext_lib).
- Tests are located in [`tests`](tests).

