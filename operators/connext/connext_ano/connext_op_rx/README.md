# ConnextRxOp

Python Holoscan operator that receives payloads broadcast over RTI Connext DDS and makes them available to Holoscan
pipelines. The operator relies on the shared helper package `holohub.connext_lib` for DDS resource management and payload
transfer.

## Parameters
- `shm_name` (str): Shared memory segment name used to expose the received payload.
- `shm_size` (int): Size of the shared memory segment in bytes.
- `event_name` (str): Synchronisation event shared with the transmitter (default `connext_rx_event`).
- `dds_domain_id` (int): DDS domain identifier (default `0`).
- `topic_name` (str): DDS topic used for registration when discovery is disabled.
- `use_discovery` (bool): Enables discovery-driven registration using `DDSDiscReceiverResourcesManager`.
- `discovery_topic_name` (str): Topic used during discovery mode (default `system_setup`).
- `output_mode` (str): Either `"bytes"` (default) to emit the raw payload or `"buffer_name"` to emit the shared-memory
  name for downstream consumers.

## Ports
- **Output** `payload`: Emits either the raw bytes or the shared-memory name, depending on `output_mode`.

## Usage

```python
from holohub.connext_ano.connext_op_rx import ConnextRxOp

fragment.add_operator(
    ConnextRxOp(
        fragment,
        name="rx",
        shm_name="holoscan_rx_buf",
        shm_size=4096,
        output_mode="bytes",
    )
)
```

When a transmitter registers buffers and publishes data, `ConnextRxOp` emits the payload to its downstream port.
