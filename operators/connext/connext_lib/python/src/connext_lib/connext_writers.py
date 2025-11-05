from typing import Optional, Any

from rti.connextdds import DataWriterQos, ReliabilityKind, \
    DurabilityKind, HistoryKind, DomainParticipant, Topic, \
    DataWriter, Publisher

from .cfg.common import ANOConfig, DDSConfig
from .endpoint_mng.user_data import (
    DDSDiscSenderResourcesManager,
)
from .payload_io.shmem_impl import MemPayloadWriter
from .comm import ConnextTx
from multiprocessing import shared_memory
import logging

#TODO: add tests for all the classes and methods in this file
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
