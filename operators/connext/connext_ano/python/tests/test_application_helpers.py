from holoscan.conditions import CountCondition
from holoscan.core import Application
from time import sleep

from connext_ano import ANOConfig, ConnextAnoRxOp, ConnextAnoTxOp, DDSConfig
from test_type import MyStringType
from test_operator_helpers import BufferSourceOp, BufferSinkOp, DDSSinkOp


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
        self._count_condition = CountCondition(self, count=10)
        self._read_count_condition = CountCondition(self, count=10)

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
        sleep(3)  # Allow some time for ANO setup

        # Source --> TX --> ShareMem --> RX --> Sink

        self.add_flow(self._source, self._tx_op, {("output", "input")})
        self.add_flow(self._rx_op, self._sink, {("output", "input")})

class ConnextApplicationDDSDummy(Application):
    """Minimal Holoscan application wiring the Connext ANO TX/RX operators."""

    def __init__(self, *, payload: str):
        super().__init__()
        self._source = None
        self._read_count_condition = None
        self._count_condition = None
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

        # Source --> TX --> ShareMem --> RX --> Sink

        self.add_flow(self._source, self._tx_op, {("output", "input")})
        self.add_flow(self._rx_op, self._sink, {("output", "input")})
        sleep(2) # Allow some time for DDS setup
