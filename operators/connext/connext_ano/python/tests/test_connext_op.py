from time import sleep

from connext_ano import ANOConfig, ConnextAnoRxOp, ConnextAnoTxOp, DDSConfig
from holoscan.core import Application, Operator, OperatorSpec
from holoscan.conditions import CountCondition
from rti.types import struct

@struct
class MyStringType:
    data: str = ""
class BufferSourceOp(Operator):
    """Simple Holoscan source that emits a fixed payload once."""

    def __init__(self, fragment, *args, payload: str, **kwargs):
        super().__init__(fragment, *args, **kwargs)
        self._payload = payload
        self._emitted_pkgs = 0

    def setup(self, spec: OperatorSpec):
        spec.output("output")

    def start(self):  # pragma: no cover - deterministic initialisation
        super().start()
        self._emitted_pkgs = 0

    def compute(self, _op_input, op_output, _context):
        self._emitted_pkgs += 1
        self._payload = self._payload + f"_#{self._emitted_pkgs}"
        op_output.emit(self._payload, "output")
       


class BufferSinkOp(Operator):
    """Sink operator that records received payloads."""

    def __init__(self, fragment, *, storage: list[str], **kwargs):
        super().__init__(fragment, **kwargs)
        self._storage = storage

    def setup(self, spec: OperatorSpec):
        spec.input("input")

    def compute(self, op_input, _op_output, _context):
        payload = op_input.receive("input")
        if payload is not None:
            self._storage.append(payload)

class DDSSinkOp(Operator):
    """Sink operator that records received payloads."""

    def __init__(self, fragment, *, storage: list[str], **kwargs):
        super().__init__(fragment, **kwargs)
        self._storage = storage

    def setup(self, spec: OperatorSpec):
        spec.input("input")

    def compute(self, op_input, _op_output, _context):
        payload = op_input.receive("input")
        if payload is not None:
            for item in payload:
                self._storage.append(str(item.data))


class ConnextApplicationANODummy(Application):
    """Minimal Holoscan application wiring the Connext ANO TX/RX operators."""

    def __init__(self, *, rx_shm_name: str, payload: str, shm_size: int = 1024):
        super().__init__()
        self._rx_shm_name = rx_shm_name
        self._payload = payload
        self._shm_size = shm_size
        self._broadcasts: list[str] = []
        self._received: list[str] = []
        self._tx_op: ConnextAnoTxOp | None = None

    @property
    def broadcasts(self) -> list[str]:
        return self._broadcasts
    
    @property
    def received(self) -> list[str]:
        return self._received

    def compose(self):
        
        # Create a count condition to limit the number of transmissions
        self._count_condition = CountCondition(self, count=3)
        self._read_count_condition = CountCondition(self, count=3)

        self._source = BufferSourceOp(self, self._count_condition, name="buffer_source", payload=self._payload)


        tx_ano_config = ANOConfig(
            shm_name="connext_tx_shm",
            shm_size=self._shm_size,
            event_name="connext_tx_event",
            enabled=True
        )
        self._tx_op = ConnextAnoTxOp(
            self,
            name="tx",
            dds_config=DDSConfig(),
            ano_config=tx_ano_config,
        )

        rx_ano_config = ANOConfig(
            shm_name=self._rx_shm_name,
            shm_size=self._shm_size,
            event_name="connext_tx_event",
            enabled=True
        )
        self._rx_op = ConnextAnoRxOp(
            self,
            self._read_count_condition,
            name="rx",
            dds_config=DDSConfig(),
            ano_config=rx_ano_config,
        )
        self._sink = BufferSinkOp(self, name="buffer_sink", storage=self._received)
        sleep(2)  # Allow some time for ANO setup

        # Source --> TX --> ShareMem --> RX --> Sink

        self.add_flow(self._source, self._tx_op, {("output", "input")})
        self.add_flow(self._rx_op, self._sink, {("output", "input")})
class ConnextApplicationDDSDummy(Application):
    """Minimal Holoscan application wiring the Connext ANO TX/RX operators."""

    def __init__(self, *, payload: str):
        super().__init__()
        self._payload = payload
        self._broadcasts: list[str] = []
        self._received: list[str] = []
        self._tx_op: None

    @property
    def broadcasts(self) -> list[str]:
        return self._broadcasts

    @property
    def received(self) -> list[str]:
        return self._received

    def compose(self):

        # Create a count condition to limit the number of transmissions
        self._count_condition = CountCondition(self, count=10)
        self._read_count_condition = CountCondition(self, count=10)

        self._source = BufferSourceOp(self, self._count_condition, name="buffer_source", payload=self._payload)


        tx_ano_config = ANOConfig(
            enabled=False
        )
        tx_dds_config = DDSConfig(
            enabled=True,
            domain_id=2,
            topic_name="MyTopic",
            topic_class=MyStringType
        )
        self._tx_op = ConnextAnoTxOp(
            self,
            name="tx",
            dds_config=tx_dds_config,
            ano_config=tx_ano_config,
        )

        rx_ano_config = ANOConfig(
            enabled=False
        )
        rx_dds_config= DDSConfig(
            enabled=True,
            domain_id=2,
            topic_name="MyTopic",
            topic_class=MyStringType
        )
        self._rx_op = ConnextAnoRxOp(
            self,
            self._read_count_condition,
            name="rx",
            dds_config=rx_dds_config,
            ano_config=rx_ano_config,
        )
        self._sink = DDSSinkOp(self, name="dds_sink", storage=self._received)
        sleep(2) # Allow some time for DDS setup

        # Source --> TX --> ShareMem --> RX --> Sink

        self.add_flow(self._source, self._tx_op, {("output", "input")})
        self.add_flow(self._rx_op, self._sink, {("output", "input")})


def test_tx_constructs_with_defaults():
    app = Application()
    op = ConnextAnoTxOp(fragment=app)
    assert op is not None


def test_rx_constructs_with_defaults():
    app = Application()
    op = ConnextAnoRxOp(fragment=app)
    assert op is not None


def test_tx_starts_and_stops():
    app = Application()
    tx = ConnextAnoTxOp(
        fragment=app,
        ano_config=ANOConfig(enabled=True),
        dds_config=DDSConfig(),
    )
    tx.start()
    tx.stop()
    assert True  # If no exceptions, the test passes


def test_rx_starts_and_stops():
    app = Application()
    rx = ConnextAnoRxOp(
        fragment=app,
        ano_config=ANOConfig(enabled=True),
        dds_config=DDSConfig(),
    )
    rx.start()
    rx.stop()
    assert True  # If no exceptions, the test passes

def test_tx_rx_discovery():

    rx_shm_name = "rx_shm_memory"

    app = Application()
    tx = ConnextAnoTxOp(
        fragment=app,
        ano_config=ANOConfig(shm_name="tx_shm_memory", enabled=True),
        dds_config=DDSConfig(),
    )
    rx = ConnextAnoRxOp(
        fragment=app,
        ano_config=ANOConfig(shm_name=rx_shm_name, enabled=True),
        dds_config=DDSConfig(),
    )

    tx.start()
    rx.start()
    sleep(2)  # Allow some time for discovery to complete

    # Check if both operators have finished the discovery process
    assert rx_shm_name in tx.connext_ano_writer.get_discovery_manager().get_destinations().values()
    tx.stop()
    rx.stop()
    sleep(2)
    assert True

def test_tx_rx_ano_integration():
    rx_shm_name = "rx_shm_memory"
    payload = "hello_holoscan"

    app_tx = ConnextApplicationANODummy(rx_shm_name=rx_shm_name, payload=payload)

    print("Running TX application...")
    app_tx.run()
    sleep(2)
    # Assert payload string in one of received string list
    received_payloads = app_tx.received
    print("Received payloads:", received_payloads)
    assert any(payload.encode("utf-8") in received for received in received_payloads)

def test_tx_rx_dds_integration():
    payload = "hello_holoscan"
    app_tx = ConnextApplicationDDSDummy(payload=payload)
    print("Running TX application...")
    app_tx.run()
    # Assert payload string in one of received string list
    received_payloads = app_tx.received
    print("Received payloads:", received_payloads)
    assert any(payload in received for received in received_payloads)


