"""Connext ANO receive operator."""

from __future__ import annotations

import logging
from typing import Optional

from holoscan.core import Operator, OperatorSpec

from connext_lib.comm import ConnextRx
from connext_lib.payload_io import MemPayloadReader
from connext_lib.system_setup import (
    DDSDiscReceiverResourcesManager,
    DDSReceiverResourcesManager,
    DummyUserType,
)

from .common import ANOConfig, DDSConfig, TransportState


class ConnextAnoRxOp(Operator):
    """Minimal Connext receive operator with ANO placeholders."""

    def __init__(
        self,
        fragment,
        *args,
        shm_name: str,
        shm_size: int,
        dds_config: Optional[DDSConfig] = None,
        ano_config: Optional[ANOConfig] = None,
        event_name: str = "connext_rx_event",
        output_mode: str = "bytes",
        **kwargs,
    ) -> None:
        super().__init__(fragment, *args, **kwargs)
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")

        self._shm_name = shm_name
        self._shm_size = shm_size
        self._event_name = event_name
        self._output_mode = output_mode

        self._dds_config = dds_config or DDSConfig()
        if self._dds_config.topic_class is None:
            self._dds_config.topic_class = DummyUserType

        self._transport_state = TransportState(ano_config or ANOConfig())

        self._dds_receiver_mgr = None
        self._payload_reader = None
        self._dds_rx = None

    # ------------------------------------------------------------------
    def setup(self, spec: OperatorSpec) -> None:
        spec.output("output")

    def start(self) -> None:
        super().start()
        self._init_dds()
        self._perform_capability_exchange()

    def stop(self) -> None:
        # Receiver side currently has no background threads, but keep hook for symmetry
        super().stop()

    # ------------------------------------------------------------------
    def compute(self, _op_input, op_output, _context) -> None:
        if self._transport_state.should_use_ano():
            # TODO-JUANCA: read from ANO completion queue and emit downstream
            self._logger.debug("ANO path not implemented yet; falling back to DDS")

        if self._dds_rx is None:
            self._logger.warning("DDS receiver not initialised, skipping output")
            return

        if not self._dds_rx.receive_buffer():
            return

        if self._output_mode == "buffer_name":
            op_output.emit(self._shm_name, "output", "std::string")
            return

        payload = self._read_shared_memory()
        if payload is not None:
            op_output.emit(payload, "output")

    # ------------------------------------------------------------------
    def _init_dds(self) -> None:
        self._logger.debug("Initialising DDS receiver resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)
        if self._dds_config.use_discovery:
            self._dds_receiver_mgr = DDSDiscReceiverResourcesManager(
                buffer_id=self._shm_name,
                user_topic_name=self._dds_config.discovery_topic_name,
                user_topic_type=self._dds_config.user_type,
                dds_domain_id=self._dds_config.domain_id,
            )
        else:
            self._dds_receiver_mgr = DDSReceiverResourcesManager(
                buffer_id=self._shm_name,
                dds_domain_id=self._dds_config.domain_id,
                topic_name=self._dds_config.topic_name,
            )
        self._payload_reader = MemPayloadReader(
            name=self._shm_name,
            size=self._shm_size,
            event_name=self._event_name,
        )
        self._dds_rx = ConnextRx(self._dds_receiver_mgr, self._payload_reader)

    def _perform_capability_exchange(self) -> None:
        # TODO-JUANCA: capture remote TX capabilities and decide whether ANO may be used
        self._transport_state.deactivate_ano()

    def _read_shared_memory(self) -> Optional[bytes]:
        try:
            import multiprocessing.shared_memory as shm

            segment = shm.SharedMemory(name=self._shm_name, create=False)
            try:
                return bytes(segment.buf[: self._shm_size])
            finally:
                segment.close()
        except FileNotFoundError:
            self._logger.warning("Shared memory '%s' not available", self._shm_name)
            return None
