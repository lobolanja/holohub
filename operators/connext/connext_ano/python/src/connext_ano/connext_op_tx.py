"""Connext ANO transmit operator."""

from __future__ import annotations

import logging
from typing import Any, Optional

from holoscan.core import Operator, OperatorSpec

from connext_lib.comm import ConnextTx
from connext_lib.payload_io import MemPayloadWriter
from connext_lib.system_setup import (
    DDSDiscSenderResourcesManager,
    DDSSenderResourcesManager,
    DummyUserType,
)

from .common import ANOConfig, DDSConfig, TransportState


class ConnextAnoTxOp(Operator):
    """Minimal Connext transmit operator with ANO fallback placeholders."""

    def __init__(
        self,
        fragment,
        *args,
        shm_name: str,
        shm_size: int,
        dds_config: Optional[DDSConfig] = None,
        ano_config: Optional[ANOConfig] = None,
        event_name: str = "connext_tx_event",
        **kwargs,
    ) -> None:
        super().__init__(fragment, *args, **kwargs)
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")

        self._shm_name = shm_name
        self._shm_size = shm_size
        self._event_name = event_name

        self._dds_config = dds_config or DDSConfig()
        if self._dds_config.topic_class is None:
            self._dds_config.topic_class = DummyUserType

        self._transport_state = TransportState(ano_config or ANOConfig())

        self._dds_sender_mgr = None
        self._payload_writer = None
        self._dds_tx = None

    # ------------------------------------------------------------------
    def setup(self, spec: OperatorSpec) -> None:
        spec.input("input")

    def start(self) -> None:
        super().start()
        self._init_dds()
        self._perform_capability_exchange()

    def stop(self) -> None:
        if self._dds_sender_mgr is not None:
            self._dds_sender_mgr.stop_processing()
        super().stop()

    # ------------------------------------------------------------------
    def compute(self, op_input, _op_output, _context) -> None:
        payload = op_input.receive("input")
        if payload is None:
            return

        if self._transport_state.should_use_ano():
            # TODO-JUANCA: send the payload through the ANO data plane
            self._logger.debug("ANO path not implemented yet; falling back to DDS")

        if self._dds_tx is None:
            self._logger.warning("DDS transmitter not initialised, dropping payload")
            return

        buffer_ref = self._buffer_reference_from_payload(payload)
        self._dds_tx.broadcast_buffer(buffer_ref)

    # ------------------------------------------------------------------
    def _init_dds(self) -> None:
        self._logger.debug("Initialising DDS sender resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)

        self._dds_sender_mgr = DDSDiscSenderResourcesManager(
            user_topic_name=self._dds_config.topic_name,
            user_topic_type=self._dds_config.topic_class,
            dds_domain_id=self._dds_config.domain_id,
        )

        self._payload_writer = MemPayloadWriter(
            name=self._shm_name,
            size=self._shm_size,
            event_name=self._event_name,
        )
        self._dds_tx = ConnextTx(self._dds_sender_mgr, self._payload_writer)

    def _perform_capability_exchange(self) -> None:
        # TODO-JUANCA: receive remote capabilities and decide whether ANO can be activated
        self._transport_state.deactivate_ano()

    def _buffer_reference_from_payload(self, payload: Any) -> str:
        """Resolve the shared-memory reference associated with the payload."""
        if isinstance(payload, str):
            return payload
        # TODO-JUANCA: define how upstream operators provide shared-memory references
        return self._shm_name
