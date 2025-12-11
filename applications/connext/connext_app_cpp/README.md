# Connext C++ Demo

This Holoscan sample demonstrates a Connext-powered roundtrip in C++. Two separate
processes exchange string payloads over either the ANO or DDS transports. YAML
configuration files control which mode to run and how each transport is
parameterized.

## Prerequisites
- Holohub environment set up with the Holoscan SDK 3.0 runtime.
- RTI Connext DDS libraries and a valid license.
  - Install the Python bindings (they stage the native shared libraries required by
    the Holoscan Connext operators even when you only run the C++ sample):
    ```sh
    pip install rti.connext==7.3.0
    ```
  - Install the debian packages for the Connext C++ libraries from
    the RTI website according to your platform.
    ```sh
    sudo apt install rti-connext-dds-7.3.0
    ```
  - source rti setenv script to set up environment variables:
    ```sh
    source /opt/rti.com/rti_connext_dds-7.3.0/resource/scripts/rtisetenv_x64Linux4gcc7.3.0.bash
    ```
  - Point `RTI_LICENSE_FILE` at the license file provided with your Connext
    distribution:
    ```sh
    export RTI_LICENSE_FILE=/path/to/rti_license.dat
    ```
- CUDA runtime libraries discoverable at runtime, either by installing CuPy (choose the
  wheel matching your CUDA toolkit, for example `cupy-cuda12x`) or exporting
  `LD_LIBRARY_PATH` to include your CUDA toolkit location:
  ```sh
  pip install cupy
  # or
  export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
  ```

## Build
Build the C++ sample from the Holohub root. The `--local` flag keeps all build
artifacts under the workspace:
```sh
./holohub build connext --language cpp --build-type debug --local
```

## Run
Start the transmitter first, then the receiver. Both processes must agree on the
transport and channel identifiers. The build drops two configuration files next
to the executable inside `holohub_bin/examples/connext/` (or the relative path you chose with
`--local`):

- `connext_receiver.yaml` configures the application in RX mode.
- `connext_sender.yaml` configures the application in TX mode.

Launch each process by passing the desired YAML file as the sole argument. The
command below assumes the default Holohub run directory (`holohub_bin`) where
the binary and configuration files are staged.

<!-- TODO: Set to the correct connext_transmitter.yaml path -->
Terminal 1 (transmitter):
```sh
./holohub run connext --language cpp \
  --run-args="connext/applications/connext/cpp/connext_sender.yaml"
```
<!-- TODO: Set to the correct connext_receiver.yaml path -->
Terminal 2 (receiver):
```sh
./holohub run connext --language cpp \
  --run-args="connext/applications/connext/cpp/connext_receiver.yaml"
```

If you run the binary directly from the build tree, point it at the staged YAML file explicitly:
```sh
./build/applications/connext/cpp/connext_sender_receiver \
  holohub_bin/examples/connext/connext_receiver.yaml
```

You can keep the YAML files under version control or customize copies on disk—just ensure
both peers load configurations with consistent transport and channel values.

Each file contains these sections:

- `demo`: Shared runtime controls. Adjust `message_count`, `message_period_ms`, and
  `discovery_wait_ms` (receiver only). The `mode` field is pre-populated.
- `payload_source`: Sets the base payload string appended by the TX path.
- `connext_tx` / `connext_rx`: Transport-specific parameters used by the Holoscan
  Connext operators. Update `enable_dds`/`enable_ano`, `domain_id`, `topic_name`,
  `ano_channel`, and related entries so both peers agree on the same transport configuration.

Keep the sender and receiver YAML files in sync for the selected transport—DDS domain
ID and topic name must match, as do ANO channel and buffer identifiers.
