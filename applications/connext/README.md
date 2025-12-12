# Connext Application simulating ANO path using DDS (C++)

This application is used for Integration porpuses with RTI Connext DDS and ANO transport using C++ Holoscan operators. But it does not implement the ANO transport, it simulates it using DDS transport.

## Dependencies
:warning: All those dependencies are already installed if you user the devcontainer provided with this application. if that is the case you can jump directly to the C++ Holoscan Application section.

This application requires the RTI Connext DDS package and a valid RTI license file.

For installing RTI Connext DDS 7.3.0 from debian packages, follow these steps:


1. **Add the RTI repository GPG key:**
  ```sh
  sudo curl -sSL -o /usr/share/keyrings/rti-official-archive.gpg \
    https://packages.rti.com/deb/official/repo.key
  ```

2. **Add the RTI APT repository to your sources list:**
  ```sh
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/rti-official-archive.gpg] \
  https://packages.rti.com/deb/official $(. /etc/os-release && echo ${VERSION_CODENAME}) main" | \
  sudo tee /etc/apt/sources.list.d/rti.list >/dev/null
  ```

3. **Accept the RTI License Agreement and install RTI Connext DDS 7.3.0:**
  ```sh
  export RTI_LICENSE_AGREEMENT_ACCEPTED=accepted
  sudo apt update
  sudo apt install -y rti-connext-dds-7.3.0
  ```

4. **Install OpenJDK if required by your workflow:**
  ```sh
  sudo apt install --no-install-recommends -y openjdk-21-jre
  ```

5. **Set the `JREHOME` environment variable for Java:**
  ```sh
  echo 'export JREHOME=$(readlink /etc/alternatives/java | sed -e "s/\/bin\/java//")' | sudo tee -a /etc/bash.bashrc
  ```

Alternatively, ensure that the CUDA toolkit libraries are available in your `LD_LIBRARY_PATH`. You can set this
environment variable as follows:
```sh
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```
Either path works—pick one and keep it consistent to avoid accidentally mixing CUDA versions.

### License reminder

RTI Connext DDS requires a valid license file to operate. You can obtain this file from the RTI customer portal or your organization’s RTI administrator. 

**Before running any Holoscan Connext component, set the `RTI_LICENSE_FILE` environment variable to the absolute path of your license file:**

```sh
export RTI_LICENSE_FILE=</path/to/rti_license.dat>
```

If this variable is not set or points to an invalid file, the application will not start.

## C++ Holoscan Application

### Build Using Holohub 

Please Notice that this instruction are for building inside the dockerfile under applications/connext/connext_app_cpp/Dockerfile
Build the C++ sample from the Holohub root. 
```sh
./holohub build connext_app_cpp --build-type debug 
```
you can use the --local flag to keep all build artifacts under the host machine instead of the container:
```sh
./holohub build connext_app_cpp --build-type debug --local
```

## Run
Start the transmitter first, then the receiver. Both processes must agree on the
transport and channel identifiers. The build drops two configuration files next
to the executable inside `build/applications/connext_app_cpp` these files are `applications/connext/connext_app_cpp/connext_receiver.yaml` and `/home/juanca/holo/holohub/applications/connext/connext_app_cpp/connext_sender.yaml`.


- `connext_receiver.yaml` configures the application in RX mode.
- `connext_sender.yaml` configures the application in TX mode.

Launch each process by passing the desired YAML file as the sole argument. The
command below assumes the default Holohub run directory (`holohub_bin`) where
the binary and configuration files are staged.

Terminal 1 (transmitter):
```sh
./holohub run connext_app_cpp --language cpp   --run-args="/workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/connext_sender.yaml" --docker-opts="-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat"
```

Terminal 2 (receiver):
```sh
./holohub run connext_app_cpp --language cpp   --run-args="/workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/connext_receiver.yaml" --docker-opts="-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat"
```

If you are using the --local flag for building you have to add it to the command for running it also:

```sh
./holohub run connext_app_cpp --language cpp   --run-args="/workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/connext_sender.yaml" --docker-opts="-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat" --local
```


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


## Building and Running Using Raw CMake

Run the application after ensuring:
- the `RTI_LICENSE_FILE` environment variable points to a valid license
- CUDA runtime libraries are available by exporting
  `LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH`


```sh
export RTI_LICENSE_FILE=</path/to/rti_license.dat>
export NDDSHOME=/opt/rti.com/rti_connext_dds-7.3.0
source $NDDSHOME/resource/scripts/rtisetenv_x64Linux4gcc7.3.0.bash
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
cmake -B build -DBUILD_TESTING=ON
cmake --build build --target connext_app_cpp
```