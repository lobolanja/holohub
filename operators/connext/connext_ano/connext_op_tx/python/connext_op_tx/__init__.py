"""Holoscan operator for broadcasting buffers over RTI Connext DDS."""

import logging
from multiprocessing import shared_memory
from typing import Optional

try:
    import numpy as _np  # noqa: F401
except ImportError:  # pragma: no cover - numpy is optional at runtime
    _np = None

from holoscan.core import Fragment, Operator, OperatorSpec

from holohub.connext_lib.comm import ConnextTx
from holohub.connext_lib.payload_io import MemPayloadWriter
from holohub.connext_lib.system_setup import (
    DDSDiscSenderResourcesManager,
    DDSSenderResourcesManager,
    DummyUserType,
)


class ConnextTxOp(Operator):
    """Broadcasts data from shared memory to DDS listeners using the Connext support library."""

    def __init__(
        self,
        fragment: Fragment,
        *args,
        shm_name: str,
        shm_size: int,
        event_name: str = "connext_tx_event",
        dds_domain_id: int = 0,
        topic_name: str = "system_setup",
        use_discovery: bool = False,
        discovery_topic_name: str = "system_setup",
        zero_pad: bool = True,
        **kwargs,
    ) -> None:
        """Create the operator.

        Parameters
        ----------
        fragment:
            Holoscan fragment owning the operator.
        shm_name:
            Name of the source shared memory segment containing the payload to broadcast.
        shm_size:
            Size (in bytes) of the shared memory segment.
        event_name:
            Synchronisation event shared with receivers when using the in-memory payload helpers.
        dds_domain_id:
            DDS domain identifier to use for discovery/advertisement.
        topic_name:
            DDS topic used by the resource manager when *not* relying on discovery.
        use_discovery:
            If ``True`` the discovery-based resource manager will be used.
        discovery_topic_name:
            DDS topic name used for discovery announcements when ``use_discovery`` is ``True``.
        zero_pad:
            When ``True`` any unused portion of the shared memory buffer is cleared between messages.
        """

        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._shm_name = shm_name
        self._shm_size = shm_size
        self._event_name = event_name
        self._zero_pad = zero_pad

        # The resource manager controls how receivers register their buffers.
        if use_discovery:
            self._sender_mgr = DDSDiscSenderResourcesManager(
                user_topic_name=discovery_topic_name,
                user_topic_type=DummyUserType,
                dds_domain_id=dds_domain_id,
            )
        else:
            self._sender_mgr = DDSSenderResourcesManager(
                dds_domain_id=dds_domain_id,
                topic_name=topic_name,
            )

        # Payload writer copies data from the local shared memory into the registered destinations.
        self._payload_writer = MemPayloadWriter(
            name=self._shm_name,
            size=self._shm_size,
            event_name=self._event_name,
        )
        self._tx = ConnextTx(self._sender_mgr, self._payload_writer)

        self._shm_owner = False
        self._shm: Optional[shared_memory.SharedMemory] = None
        self._ensure_shared_memory()

        super().__init__(fragment, *args, **kwargs)

    def setup(self, spec: OperatorSpec) -> None:
        spec.input("payload")

    def compute(self, op_input, _op_output, _context) -> None:
        payload = op_input.receive("payload")
        raw = self._as_bytes(payload)
        if len(raw) > self._shm_size:
            raise ValueError(
                f"Payload of {len(raw)} bytes exceeds shared memory capacity ({self._shm_size} bytes)"
            )

        self._logger.debug("Writing %d bytes to shared memory '%s'", len(raw), self._shm_name)
        self._shm.buf[: len(raw)] = raw
        if self._zero_pad and len(raw) < self._shm_size:
            self._shm.buf[len(raw) : self._shm_size] = b"\x00" * (self._shm_size - len(raw))

        self._tx.broadcast_buffer(self._shm_name)

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
                "Re-using existing shared memory '%s' (size=%d bytes)",
                self._shm.name,
                self._shm_size,
            )

    def _shutdown(self) -> None:
        if self._shm is not None:
            try:
                self._shm.close()
                if self._shm_owner:
                    self._shm.unlink()
            finally:
                self._shm = None

    @staticmethod
    def _as_bytes(payload) -> bytes:
        if payload is None:
            return b""
        if isinstance(payload, bytes):
            return payload
        if isinstance(payload, bytearray):
            return bytes(payload)
        if isinstance(payload, memoryview):
            return payload.tobytes()
        if hasattr(payload, "to_numpy"):
            array = payload.to_numpy()
            return array.tobytes()
        if _np is not None and hasattr(payload, "__array__"):
            return _np.asarray(payload).tobytes()
        if isinstance(payload, str):
            return payload.encode()
        raise TypeError(f"Unsupported payload type: {type(payload)!r}")

    def __del__(self):  # pragma: no cover - best-effort cleanup
        self._shutdown()


__all__ = ["ConnextTxOp"]
