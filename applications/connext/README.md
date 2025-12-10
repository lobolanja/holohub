# Connext ANO Loopback Application (Python)

This example Holoscan application demonstrates how to integrate the Python Connext ANO transmit and receive operators
in a single fragment. A configurable source operator emits a handful of string payloads that travel through the
Connext ANO transmitter and back into the process via the Connext ANO receiver. The received payloads are stored by a
sink operator so they can be validated in tests or further processed by additional stages.

## Dependencies
This application requires the RTI Connext DDS Python package and a valid RTI license file. You can install the package
using pip:
```sh
pip install rti.connext==7.3.0
```
Make sure to set the `RTI_LICENSE_FILE` environment variable to point to your RTI license file before running the
application.

CuPy is also required to provide access to the CUDA runtime libraries. You can install CuPy via pip:
```sh
pip install cupy
```
Select the wheel that matches your CUDA toolkit (for example `cupy-cuda12x` for CUDA 12). Refer to the
[CuPy installation matrix](https://docs.cupy.dev/en/stable/install.html) if the default wheel does not match the
toolkit bundled with your system or container image.

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
export RTI_LICENSE_FILE=/path/to/rti_license.dat
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
cmake -B build -DHOLOHUB_BUILD_PYTHON=ON -DBUILD_TESTING=ON
cmake --build build --target connext_lib_python connext_ano_python
python3 applications/connext/python/src/run_demo.py --transport ano
```

Pass `--transport dds` to exercise DDS-based delivery. Building the Holohub application target automatically stages the
`connext_ano` and `connext_lib` Python packages before copying the demo sources into the build tree.

To launch through the Holohub helper script instead of invoking Python directly, run:

```sh
./holohub run connext --language python --run-args="--transport ano"
```

Use `--run-args="--transport dds"` to switch to DDS networking mode.

### C++ application

Please refer to the C++ specific instructions in `applications/connext/cpp/README.md`.

## Container Support

A reference Dockerfile is provided to simplify deployment. Build and run the application inside the container with:

```sh
./holohub run-container connext \
  --docker-opts "-e RTI_LICENSE_FILE=/workspace/licenses/rti_license.dat \
                 -v $HOME/rti_license.dat:/workspace/licenses/rti_license.dat"
```

The Docker image installs the CUDA runtime libraries and the `rti.connext==7.3.0` Python bindings. Mount your RTI
license file into the container (as shown above) or override `RTI_LICENSE_FILE` to point at another location.

## Developing
If you are actively developing this application or the connext operators you can run directly pytests against the source
files without needing to build the container image. From the holohub root directory run:

```sh
export RTI_LICENSE_FILE=/path/to/rti_license.dat
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
pytest applications/connext/python/tests
```
Make sure you have installed the required dependencies in your Python environment before running the tests.