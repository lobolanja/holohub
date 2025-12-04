# Connext C++ Demo

This Holoscan sample demonstrates a Connext-powered roundtrip in C++. Two separate
processes exchange string payloads over either the ANO or DDS transports. The
transmitter (`--mode=tx`) publishes incrementing messages, while the receiver
(`--mode=rx`) prints every payload as it arrives.

## Prerequisites
- Holohub environment set up with the Holoscan SDK 3.0 runtime.
- RTI Connext DDS libraries and a valid license.
  - Install the Python bindings (they stage the native shared libraries required by
    the Holoscan Connext operators):
    ```sh
    pip install rti.connext==7.3.0
    ```
  - Point `RTI_LICENSE_FILE` at the license file provided with your Connext
    distribution:
    ```sh
    export RTI_LICENSE_FILE=/path/to/rti_license.dat
    ```
- CUDA runtime libraries discoverable at runtime, either by installing CuPy or
  exporting `LD_LIBRARY_PATH` to include your CUDA toolkit location:
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
Start the receiver first, then the transmitter. Both processes must agree on the
transport and channel identifiers.

### ANO roundtrip example
Terminal 1 (receiver):
```sh
./holohub run connext --language cpp \
  --run-args="--mode=rx --use-ano --ano-channel=demo_channel --ano-buffer-id=demo_buffer"
```

Terminal 2 (transmitter):
```sh
./holohub run connext --language cpp \
  --run-args="--mode=tx --use-ano --ano-channel=demo_channel --ano-buffer-id=demo_buffer --message-count=5"
```

The transmitter emits five payloads with the default base string
`hello_holoscan`. The receiver prints each arrival to stdout.

### DDS roundtrip example
If you prefer to exercise DDS networking, provide the `--use-dds` flag and
assign both applications the same domain ID and topic name:

Terminal 1:
```sh
./holohub run connext --language cpp \
  --run-args="--mode=rx --use-dds --dds-domain-id=1 --dds-topic-name=ConnextDemoTopic"
```

Terminal 2:
```sh
./holohub run connext --language cpp \
  --run-args="--mode=tx --use-dds --dds-domain-id=1 --dds-topic-name=ConnextDemoTopic --message-count=5"
```

Increase `--message-count` or omit it entirely for continuous publishing.
Receiver-side `--discovery-wait-ms` (default 3000) controls how long the
application allows DDS discovery to complete before pulling messages.

## Additional options
Key flags exposed by the binary (see `connext_common.cpp` for the full list):
- `--payload` sets the base string appended with an incrementing counter.
- `--message-period-ms` throttles continuous publishing.
- `--destination` passes an optional destination reference to the Connext
  transport layer.
- `--ano-max-payload` adjusts the maximum payload size negotiated with ANO.

Run `./holohub run connext --language cpp --run-args="--help"` to see the full
usage text.
