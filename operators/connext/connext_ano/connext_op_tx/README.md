# ConnextTxOp

Python Holoscan operator that broadcasts a shared-memory buffer to RTI Connext DDS listeners. The operator wraps the
`holohub.connext_lib` helpers, handling resource registration and memory-to-memory transfers between local buffers and
remote receivers.

## Parameters
- `shm_name` (str): Name of the shared memory segment holding the payload to broadcast.
- `shm_size` (int): Size of the shared memory segment in bytes.
- `event_name` (str): Synchronisation event shared by the in-memory helpers. Defaults to `connext_tx_event`.
- `dds_domain_id` (int): DDS domain identifier (default `0`).
- `topic_name` (str): DDS topic used when resource registration happens via topics rather than discovery.
- `use_discovery` (bool): When `True`, enables discovery-based registration via `DDSDiscSenderResourcesManager`.
- `discovery_topic_name` (str): Topic used during discovery mode (default `system_setup`).
- `zero_pad` (bool): Clears the unused portion of the shared memory buffer after each transmit (default `True`).

## Ports
- **Input** `payload`: Bytes-like object (bytes, bytearray, memoryview) or tensor convertible to bytes. The payload is
  copied into the local shared memory segment before being published to all registered receivers.

## Usage

```python
from holohub.connext_ano.connext_op_tx import ConnextTxOp

# fragment: holoscan.core.Fragment
fragment.add_operator(
    ConnextTxOp(
        fragment,
        name="tx",
        shm_name="holoscan_tx_buf",
        shm_size=4096,
        event_name="holoscan_connext_event",
    )
)
```

Pipe a tensor or bytes into the `payload` port to broadcast the data. Receivers created with
`ConnextRxOp` (below) or custom code that uses `holohub.connext_lib` will receive the payload.
