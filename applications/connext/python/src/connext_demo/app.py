import logging
from dataclasses import dataclass
from typing import Iterable, List

from holoscan.conditions import CountCondition
from holoscan.core import Application, Operator, OperatorSpec
from rti.types import struct

from connext_ano import ANOConfig, ConnextAnoRxOp, ConnextAnoTxOp, DDSConfig

logger = logging.getLogger(__name__)


@struct
class _DemoTopic:
    """DDS payload used when the demo runs in network (DDS) mode."""

    data: str = ""


@dataclass
class DemoAppConfig:
    """Runtime configuration for the Connext demo application."""

    payload: str = "hello_holoscan"
    message_count: int = 10
    shm_name_tx: str = "connext_demo_tx"
    shm_name_rx: str = "connext_demo_rx"
    shm_event_name: str = "connext_demo_event"
    shm_size: int = 4096
    dds_domain_id: int = 2
    dds_topic_name: str = "ConnextDemoTopic"
    use_dds: bool = False


class _PayloadSourceOp(Operator):
    """Holoscan source that emits a string payload a fixed number of times."""

    def __init__(self, fragment, *, payload: str, **kwargs):
        super().__init__(fragment, **kwargs)
        self._base_payload = payload
        self._emitted = 0

    def setup(self, spec: OperatorSpec):
        spec.output("output")

    def start(self):
        super().start()
        self._emitted = 0

    def compute(self, _op_input, op_output, _context):
        self._emitted += 1
        message = f"{self._base_payload}_#{self._emitted}"
        op_output.emit(message, "output")


class _PayloadSinkOp(Operator):
    """Sink that stores each payload received from Connext ANO operators."""

    def __init__(self, fragment, *, storage: List, **kwargs):
        super().__init__(fragment, **kwargs)
        self._storage = storage

    def setup(self, spec: OperatorSpec):
        spec.input("input")

    def compute(self, op_input, _op_output, _context):
        payload = op_input.receive("input")
        if payload is not None:
            self._storage.append(payload)


class ConnextAnoLoopbackApp(Application):
    """Sample Holoscan application that loops payloads through the Connext ANO operators."""

    def __init__(self, config: DemoAppConfig | None = None):
        super().__init__()
        self._config = config or DemoAppConfig()
        self._received: List = []

    @property
    def received_payloads(self) -> Iterable:
        """Return the payloads captured by the sink operator."""

        return tuple(self._received)

    def compose(self):
        cfg = self._config

        source_condition = CountCondition(self, count=cfg.message_count)
        sink_condition = CountCondition(self, count=cfg.message_count)

        source = _PayloadSourceOp(
            self,
            payload=cfg.payload,
            condition=source_condition,
        )

        ano_tx_config = ANOConfig(
            shm_name=cfg.shm_name_tx,
            shm_size=cfg.shm_size,
            event_name=cfg.shm_event_name,
            enabled=not cfg.use_dds,
        )
        dds_tx_config = DDSConfig(
            enabled=cfg.use_dds,
            domain_id=cfg.dds_domain_id,
            topic_name=cfg.dds_topic_name,
            topic_class=_DemoTopic,
        )
        tx = ConnextAnoTxOp(
            self,
            name="connext_tx",
            ano_config=ano_tx_config,
            dds_config=dds_tx_config,
        )

        ano_rx_config = ANOConfig(
            shm_name=cfg.shm_name_rx,
            shm_size=cfg.shm_size,
            event_name=cfg.shm_event_name,
            enabled=not cfg.use_dds,
        )
        dds_rx_config = DDSConfig(
            enabled=cfg.use_dds,
            domain_id=cfg.dds_domain_id,
            topic_name=cfg.dds_topic_name,
            topic_class=_DemoTopic,
        )
        rx = ConnextAnoRxOp(
            self,
            sink_condition,
            name="connext_rx",
            ano_config=ano_rx_config,
            dds_config=dds_rx_config,
        )

        sink = _PayloadSinkOp(self, name="demo_sink", storage=self._received)

        self.add_flow(source, tx, {("output", "input")})
        self.add_flow(rx, sink, {("output", "input")})

        logger.info(
            "Connext ANO loopback configured. transport=%s payload='%s' iterations=%d",
            "dds" if cfg.use_dds else "ano",
            cfg.payload,
            cfg.message_count,
        )
