"""Holoscan operator that reads buffers advertised over RTI Connext DDS."""

import logging
from multiprocessing import shared_memory
from typing import Optional

from holoscan.core import Fragment, Operator, OperatorSpec

from holohub.connext_lib.comm import ConnextRx
from holohub.connext_lib.payload_io import MemPayloadReader
from holohub.connext_lib.system_setup import (
    DDSDiscReceiverResourcesManager,
    DDSReceiverResourcesManager,
    DummyUserType,
)


class ConnextRxOp(Operator):
    """Receives payloads from Connext transmitters and exposes them to Holoscan pipelines."""

    def __init__(
        self,
        fragment: Fragment,
        *args,
        shm_name: str,
        shm_size: int,
        event_name: str = "connext_rx_event",
        dds_domain_id: int = 0,
        topic_name: str = "system_setup",
        use_discovery: bool = False,
        discovery_topic_name: str = "system_setup",
        output_mode: str = "bytes",
        **kwargs,
    ) -> None:
        """Create the operator.

        Parameters mirror :class:`~connext_op_tx.ConnextTxOp` so both ends can be configured symmetrically.

        ``output_mode`` controls what is emitted for each successful receive:

        - ``"bytes"`` (default): emit a ``bytes`` object containing ``shm_size`` bytes.
        - ``"buffer_name"``: emit the shared-memory name so downstream operators can attach directly.
        """

        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._shm_name = shm_name
        self._shm_size = shm_size
        self._event_name = event_name
        self._output_mode = output_mode

        if use_discovery:
            self._receiver_mgr = DDSDiscReceiverResourcesManager(
                buffer_id=self._shm_name,
                user_topic_name=discovery_topic_name,
                user_topic_type=DummyUserType,
                dds_domain_id=dds_domain_id,
            )
        else:
            self._receiver_mgr = DDSReceiverResourcesManager(
                buffer_id=self._shm_name,
                dds_domain_id=dds_domain_id,
                topic_name=topic_name,
            )

        self._payload_reader = MemPayloadReader(
            name=self._shm_name,
            size=self._shm_size,
            event_name=self._event_name,
        )
        self._rx = ConnextRx(self._receiver_mgr, self._payload_reader)

        self._shm_owner = False
        self._shm: Optional[shared_memory.SharedMemory] = None
        self._ensure_shared_memory()

        super().__init__(fragment, *args, **kwargs)

    def setup(self, spec: OperatorSpec) -> None:
        spec.output("payload")

    def compute(self, _op_input, op_output, _context) -> None:
        if not self._rx.receive_buffer():
            return

        if self._output_mode == "buffer_name":
            op_output.emit(self._shm_name, "payload", "std::string")
            return

        payload = bytes(self._shm.buf[: self._shm_size])
        op_output.emit(payload, "payload")

    def stop(self) -> None:
        super().stop()
        self._shutdown()

    def _ensure_shared_memory(self) -> None:
        try:
            self._shm = shared_memory.SharedMemory(
                name=self._shm_name,
                create=True,
                size=self._shm_size,
            )
            self._shm_owner = True
            self._logger.info(
                "Created shared memory '%s' with size %d bytes", self._shm_name, self._shm_size
            )
        except FileExistsError:
            self._shm = shared_memory.SharedMemory(name=self._shm_name, create=False)
            self._logger.info(
                "Re-using existing shared memory '%s'", self._shm.name
            )

    def _shutdown(self) -> None:
        if self._shm is not None:
            try:
                self._shm.close()
                if self._shm_owner:
                    self._shm.unlink()
            finally:
                self._shm = None

    def __del__(self):  # pragma: no cover - best-effort cleanup
        self._shutdown()


__all__ = ["ConnextRxOp"]
