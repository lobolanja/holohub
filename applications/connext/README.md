# Connext ANO Applications (Python a C++)

This example Holoscan application demonstrates how to:
- integrate the Python Connext ANO transmit and receive operators in a single Holoscan Application
- integrate the C++ Connext ANO transmit and receive operators, running in two different Holoscan Applications.
A configurable source operator emits a handful of string payloads that travel through the
Connext ANO transmitter and back into the process via the Connext ANO receiver. The received payloads are stored by a sink operator so they can be validated in tests or further processed by additional stages.

## Dependencies
This application requires the RTI Connext DDS (C++ and Python) package and a valid RTI license file. You can install the package

Make sure to set the `RTI_LICENSE_FILE` environment variable to point to your RTI license file before running the
application.

Alternatively, ensure that the CUDA toolkit libraries are available in your `LD_LIBRARY_PATH`. You can set this
environment variable as follows:
```sh
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```
Either path works—pick one and keep it consistent to avoid accidentally mixing CUDA versions.

### License reminder

RTI requires a valid license file to run the Connext middleware. Obtain the license through the RTI customer portal or
your organization’s RTI administrator, then set `RTI_LICENSE_FILE` to the absolute path of the license file before
launching any Holoscan Connext component.

## Building and Running

Run the application after ensuring
- RTI Connext DDS Python package is installed: `pip install rti.connext==7.3.0`
- the `RTI_LICENSE_FILE` environment variable points to a valid license
- CUDA runtime libraries are available either through CuPy (`pip install cupy`) or by exporting
  `LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH`

Developers frequently work inside a virtual environment. If you do, remember to activate it before running the commands
below so both the Holoscan dependencies and Connext bindings resolve correctly.

```sh
export RTI_LICENSE_FILE=</path/to/rti_license.dat>
export NDDSHOME=/opt/rti.com/rti_connext_dds-7.3.0
source $NDDSHOME/resource/scripts/rtisetenv_x64Linux4gcc7.3.0.bash
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
cmake -B build -DBUILD_TESTING=ON
cmake --build build --target connext_app_cpp
```

### C++ Holoscan Application

## Build
Build the C++ sample from the Holohub root. The `--local` flag keeps all build
artifacts under the workspace:
```sh
./holohub build connext_app_cpp --build-type debug 
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







## Container Support

<!--TODO-JUANCA: update this docker file with the changes in the main one -->

A reference Dockerfile is provided to simplify deployment. Build and run the application inside the container with:

```sh
./holohub run-container connext \
  --docker-opts "-e RTI_LICENSE_FILE=/workspace/licenses/rti_license.dat \
                 -v $HOME/rti_license.dat:/workspace/licenses/rti_license.dat"
```

The Docker image installs the CUDA runtime libraries and the `rti.connext==7.3.0` Python bindings. Mount your RTI
license file into the container (as shown above) or override `RTI_LICENSE_FILE` to point at another location.
