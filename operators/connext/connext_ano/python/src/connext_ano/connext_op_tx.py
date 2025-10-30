"""Connext ANO transmit operator."""

from __future__ import annotations

import logging
from multiprocessing import shared_memory
from typing import Any, Optional

from holoscan.core import Operator, OperatorSpec

from connext_lib.comm import ConnextTx
from connext_lib.payload_io import MemPayloadWriter
from connext_lib.payload_io.mem_payload_io import _MemFileLock
from connext_lib.system_setup import (
    DDSDiscSenderResourcesManager,
    DDSSenderResourcesManager,
    DummyUserType,
)
from rti.connextdds import DomainParticipant, Topic, Publisher, DataWriter, DataWriterQos, ReliabilityKind, DurabilityKind, HistoryKind

from .common import ANOConfig, DDSConfig

class ConnextAnoWriter():
    """Manage the initialization of the Connext ANO writer."""

    def __init__(self,
        dds_config: Optional[DDSConfig] = None,
        ano_config: Optional[ANOConfig] = None
    ) -> None:
        logging.basicConfig(level=logging.INFO)
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._dds_config = dds_config or DDSConfig()
        self._ano_config = ano_config or ANOConfig()
        self._discovery_manager = None # manages DDS discovery
        self._payload_writer = None
        self._payload_tx = None

        # Initialize ANO shared memory and Connext TX
        self._init_ano()
        self._init_connext_tx()

    # ------------------------------------------------------------------
    def _init_connext_tx(self) -> None:
        self._logger.debug("Initialising DDS sender resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)
        #TODO: add DISC prefix to topic name
        self._discovery_manager = DDSDiscSenderResourcesManager(
            user_topic_name=self._dds_config.topic_name,
            user_topic_type=self._dds_config.topic_class,
            dds_domain_id=self._dds_config.domain_id,
        )

        self._payload_writer = MemPayloadWriter(
            name=self._ano_config.shm_name,
            size=self._ano_config.shm_size,
            event_name=self._ano_config.event_name
        )
        self._payload_tx = ConnextTx(self._discovery_manager, self._payload_writer)

    def _init_ano(self) -> None:
        # Create writer shared memory segment 
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
    def get_payload_writer(self) -> MemPayloadWriter:
        return self._payload_writer
    # TODO: change MemPayloadWriter to its interface
    def get_discovery_manager(self) -> DDSDiscSenderResourcesManager:
        return self._discovery_manager
    
    def start(self) -> None:
        pass

    def stop(self) -> None:
        try:
            self._shm.close()
            self._shm.unlink()
            self._logger.debug("Released shared memory segment '%s'", self._ano_config.shm_name)
        except FileNotFoundError:
            self._logger.debug("Shared memory segment '%s' already unlinked",
                               self._ano_config.shm_name)
    
    def write_buffer(self, payload) -> None:
        """Write the contents of src_data_ref to all registered buffers."""
        if self._payload_tx is None:
            self._logger.warning("DDS transmitter not initialised, cannot write buffer")
            return

        self._buffer_reference_from_payload(payload)
        self._payload_tx.broadcast_buffer()

    def _buffer_reference_from_payload(self, payload: Any) -> None:
        """Copy the payload into the shared-memory buffer and return the buffer reference."""
        if not self._payload_writer:
            self._logger.warning("Shared-memory writer not initialised, cannot copy payload")
        
        self._payload_writer.set_buffer(payload)

class ConnextDDSWriter:
    """Manage the initialization of the Connext DDS writer."""

    def __init__(self,
        dds_config: Optional[DDSConfig] = None,
    ) -> None:
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._dds_config = dds_config or DDSConfig()
        self._dds_writer = None

        self._init_dds_writer()

    # ------------------------------------------------------------------

    def _configure_dds_writer(self) -> None:
        # Create QoS with strict reliability
        writer_qos = DataWriterQos()
        writer_qos.reliability.kind = ReliabilityKind.RELIABLE
        writer_qos.durability.kind = DurabilityKind.TRANSIENT_LOCAL   # or PERSISTENT/TRANSIENT as needed
        writer_qos.history.kind = HistoryKind.KEEP_ALL

        return writer_qos

    def _init_dds_writer(self) -> None:
        self._logger.debug("Initialising DDS sender resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)

        dds_participant=DomainParticipant(self._dds_config.domain_id)
        topic = Topic(dds_participant, self._dds_config.topic_name, self._dds_config.topic_class)
        self._dds_writer = DataWriter(Publisher(dds_participant), topic, self._configure_dds_writer())

    def write_message(self, message: Any) -> None:
        """Write the given message via DDS."""
        if not self._dds_writer:
            self._logger.warning("DDS writer not initialised, cannot write message")
            return

        self._dds_writer.write(message)


class ConnextAnoTxOp(Operator):
    """Minimal Connext transmit operator with ANO fallback placeholders."""

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

        self.connext_ano_writer = None # Placeholder for Connext ANO writer
        self.connext_dds_writer = None # Placeholder for Connext DDS writer

        self._dds_config = dds_config or DDSConfig()
        if self._dds_config.topic_class is None:
            self._dds_config.topic_class = DummyUserType

        self._ano_config = ano_config or ANOConfig()

        if self._ano_config.enabled:
            self._logger.info("Connext ANO TX enabled with transport '%s'", self._ano_config.transport)
            self.connext_ano_writer = ConnextAnoWriter(
                dds_config=self._dds_config,
                ano_config=self._ano_config
            )
        else:
            self._logger.info("Connext ANO TX disabled, operator will not transmit data")

        if self._dds_config.enabled:
            self._logger.info("DDS path enabled (domain=%s topic=%s)",
                                  self._dds_config.domain_id, self._dds_config.topic_name)
            self.connext_dds_writer = ConnextDDSWriter(dds_config=self._dds_config)
        else:
            self._logger.info("DDS path disabled")

    # ------------------------------------------------------------------
    def setup(self, spec: OperatorSpec) -> None:
        spec.input("input")

    def start(self) -> None:
        super().start()
        if self._ano_config.enabled:
            self.connext_ano_writer.start()


    def stop(self) -> None:
        if self._ano_config.enabled:
            self.connext_ano_writer.stop()
        super().stop()

    # ------------------------------------------------------------------
    def compute(self, op_input, _op_output, _context) -> None:
        payload = op_input.receive("input")
        if payload is None:
            return

        if self._ano_config.enabled:
            self._logger.info("Connext ANO TX write payload '%s'", payload)
            self.connext_ano_writer.write_buffer(payload)

        if self._dds_config.enabled:
            self._logger.info("DDS TX write payload '%s'", payload)
            message = self._dds_config.topic_class(data=payload)
            self.connext_dds_writer.write_message(message)
        else:
            self._logger.info("No DDS data source configured, skipping DDS write")
