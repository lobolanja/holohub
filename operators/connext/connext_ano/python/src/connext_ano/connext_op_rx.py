"""Connext ANO receive operator."""

from __future__ import annotations

import logging
from multiprocessing import shared_memory
from typing import Optional

from holoscan.core import Operator, OperatorSpec

from connext_lib.comm import ConnextRx
from connext_lib.payload_io import MemPayloadReader
from connext_lib.system_setup import (
    DDSDiscReceiverResourcesManager,
    DDSReceiverResourcesManager,
    DummyUserType,
)
from rti.connextdds import Subscriber, DomainParticipant, Topic, DataReader, \
    DataReaderQos, ReliabilityKind, DurabilityKind, HistoryKind

from .common import ANOConfig, DDSConfig, TransportState
class ConnextAnoReader():
    """Manage the initialization of the Connext ANO reader."""

    def __init__(
        self,
        dds_config: Optional[DDSConfig] = None,
        ano_config: Optional[ANOConfig] = None
    ) -> None:
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._dds_config = dds_config or DDSConfig()
        self._ano_config = ano_config or ANOConfig()
        self._discovery_manager = None # manages DDS discovery
        self._payload_reader = None
        self._payload_rx = None

        # Create reader shared memory segment
        try:
            self._shm = shared_memory.SharedMemory(
                name=self._ano_config.shm_name,
                create=True,
                size=self._ano_config.shm_size
            )
            self._logger.debug("Created shared memory segment '%s' of size %d bytes",
                               self._ano_config.shm_name, self._ano_config.shm_size)
        except FileExistsError:
            self._shm = shared_memory.SharedMemory(
                name=self._ano_config.shm_name,
                create=False
            )
            self._logger.debug("Attached to existing shared memory segment '%s'",
                               self._ano_config.shm_name)

        self._init_dds()

    def __del__(self) -> None:
        try:
            self._shm.close()
            self._shm.unlink()
            self._logger.debug("Released shared memory segment '%s'", self._ano_config.shm_name)
        except FileNotFoundError:
            self._logger.debug("Shared memory segment '%s' already unlinked",
                               self._ano_config.shm_name)

    def get_shm(self) -> shared_memory.SharedMemory:
        return self._shm

    def get_payload_reader(self) -> MemPayloadReader:
        return self._payload_reader
    # TODO: change MemPayloadReader to its interface

    # ------------------------------------------------------------------
    def _init_dds(self) -> None:
        self._logger.debug("Initialising DDS receiver resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)
        self._discovery_manager = DDSDiscReceiverResourcesManager(
            buffer_id=self._ano_config.shm_name,
            user_topic_name=self._dds_config.topic_name,
            user_topic_type=self._dds_config.topic_class,
            dds_domain_id=self._dds_config.domain_id,
        )
        
        self._payload_reader = MemPayloadReader(
            name=self._ano_config.shm_name,
            size=self._ano_config.shm_size,
            event_name=self._ano_config.event_name,
        )
        self._payload_rx = ConnextRx(self._discovery_manager, self._payload_reader)

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

    def read_buffer(self) -> bool:
        if self._payload_rx is None:
            self._logger.warning("DDS receiver not initialised, cannot read buffer")
            return False

        return self._payload_rx.receive_buffer()
class ConnextDDSReader:
    """Manage the initialization of the Connext DDS reader."""

    def __init__(
        self,
        dds_config: Optional[DDSConfig] = None,
    ) -> None:
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._dds_config = dds_config or DDSConfig()
        self._dds_reader = None

        self._init_dds()

    def _configure_datareader_qos(self) -> None:
        # Create QoS with strict reliability
        reader_qos = DataReaderQos()
        reader_qos.reliability.kind = ReliabilityKind.RELIABLE
        reader_qos.durability.kind = DurabilityKind.VOLATILE   # or PERSISTENT/TRANSIENT as needed
        reader_qos.history.kind = HistoryKind.KEEP_ALL

        return reader_qos

    def _init_dds(self) -> None:
        self._logger.debug("Initialising DDS receiver resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)
        dds_participant=DomainParticipant(self._dds_config.domain_id)
        topic = Topic(dds_participant, self._dds_config.topic_name, self._dds_config.topic_class)
        self._dds_reader = DataReader(Subscriber(dds_participant), topic, self._configure_datareader_qos())

    def read_samples(self):
        if self._dds_reader is None:
            self._logger.warning("DDS receiver not initialised, cannot read samples")
            return []

        samples = self._dds_reader.take()
        valid_samples = [sample.data for sample in samples if sample.info.valid]
        return valid_samples
class ConnextAnoRxOp(Operator):
    """Minimal Connext receive operator with ANO placeholders."""

    def __init__(
        self,
        fragment,
        *args,
        dds_config: Optional[DDSConfig] = None,
        ano_config: Optional[ANOConfig] = None,
        **kwargs,
    ) -> None:
        super().__init__(fragment, *args, **kwargs)
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._dds_participant = None  # Placeholder for future DDS participant management
        self._dds_reader = None  # Placeholder for future DDS reader management
        self._connext_ano_reader = None # Placeholder for Connext ANO reader
        self._connext_dds_reader = None # Placeholder for Connext DDS reader

        self._dds_config = dds_config or DDSConfig()
        if self._dds_config.topic_class is None:
            self._dds_config.topic_class = DummyUserType

        self._ano_config = ano_config or ANOConfig()

        if self._ano_config.enabled:
            self._logger.info("Connext ANO enabled with transport '%s'", self._ano_config.transport)
            self._connext_ano_reader = ConnextAnoReader(self._dds_config, self._ano_config)
        else:
            self._logger.info("Connext ANO disabled, falling back to DDS only if enabled")

            if self._dds_config.enabled:
                self._logger.info("DDS path enabled (domain=%s topic=%s)",
                                  self._dds_config.domain_id, self._dds_config.topic_name)
                self._connext_dds_reader = ConnextDDSReader(dds_config=self._dds_config)

            else:
                self._logger.info("DDS path disabled. There are no data sources available.")

    # ------------------------------------------------------------------
    def setup(self, spec: OperatorSpec) -> None:
        spec.output("output")

    def start(self) -> None:
        super().start()


    def stop(self) -> None:
        # Receiver side currently has no background threads, but keep hook for symmetry
        super().stop()

    # ------------------------------------------------------------------
    def compute(self, _op_input, op_output, _context) -> None:
        payload = None
        if self._connext_ano_reader is None:
            self._logger.info("Connext Ano Path not initialised")
            if self._dds_config.enabled:
                self._logger.info("DDS path enabled, reading from DDS")
                payload = self._connext_dds_reader.read_samples()
                op_output.emit(payload, "output")
            else:
                self._logger.warning("DDS path and ANO path disabled, no data source available")
        else:
            if self._connext_ano_reader.read_buffer():
                self._logger.info("Connext Ano path read")
                payload = self._connext_ano_reader.get_shm().buf.tobytes()
                if payload:
                    op_output.emit(payload, "output")

