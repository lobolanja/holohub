# Connext Ops Tests

This directory contains the C++ integration tests for the Connext Rx/Tx operators. They validate the interaction between
the native operators, the shared helper library, and the RTI Connext middleware.

## Build Instructions
1. Ensure the RTI SDK and license are configured:
   ```sh
   export NDDSHOME=/opt/rti/rti_connext_dds-7.3.0
   export RTI_LICENSE_FILE=/path/to/rti_license.dat
   export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH  # or install CuPy for CUDA runtime access
   ```
2. Configure the Connext operators with testing enabled:
   ```sh
   cmake -B build -DBUILD_TESTING=ON
   cmake --build build --target connext_ops_cpp_tests
   ```

## Running the Tests

Execute the registered CTest target (from the same build tree):

```sh
ctest --test-dir build -R test_connext_rx_integration -V
```

The helper target `connext_ops_cpp_tests` builds the `test_connext_rx_integration` executable so you can run it directly
if preferred. Keep the RTI environment variables exported whenever the test is launched.

