# Connext Advanced Networking Operators (ANO)

This directory contains Python Holoscan operators that build on the `holohub.connext_lib` support library to integrate
with RTI Connext DDS. Two complementary operators are provided:

- [`connext_op_tx`](connext_op_tx): Broadcasts a shared-memory buffer to all registered DDS receivers.
- [`connext_op_rx`](connext_op_rx): Listens for broadcasts and exposes the received payload to Holoscan pipelines.

Both operators share configuration parameters such as the shared-memory name, buffer size, DDS domain, and optional
resource-discovery mode. Refer to the individual READMEs for detailed parameter documentation and usage examples.

The operators expect the Connext support library to be available, which is handled automatically when Holohub is built
with `HOLOHUB_BUILD_PYTHON=ON`.
