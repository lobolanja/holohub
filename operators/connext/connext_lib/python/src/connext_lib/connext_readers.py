from typing import Optional

from rti.connextdds import DataReaderQos, ReliabilityKind, DurabilityKind, HistoryKind, DomainParticipant, Topic, \
    DataReader, Subscriber

from .cfg.common import DDSConfig,ANOConfig

from multiprocessing import shared_memory

import logging

from .comm import ConnextRx
from .endpoint_mng.user_data import DDSDiscReceiverResourcesManager
from .payload_io.shmem_impl import MemPayloadReader

#TODO: add tests for all the classes and methods in this file
class ConnextAnoReader:
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

    # ------------------------------------------------------------------
    def _init_dds(self) -> None:
        self._logger.debug("Initialising DDS receiver resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)
        self._discovery_manager = DDSDiscReceiverResourcesManager(
            buffer_id=self._ano_config.shm_name,
            #TODO: add DISC prefix to topic name
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
    def get_data(self) -> bytes:
        return self._shm.buf.tobytes()

    def get_payload_reader(self) -> MemPayloadReader:
        return self._payload_reader
    # TODO: change MemPayloadReader to its interface

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
        reader_qos.durability.kind = DurabilityKind.TRANSIENT_LOCAL   # or PERSISTENT/TRANSIENT as needed
        reader_qos.history.kind = HistoryKind.KEEP_ALL

        return reader_qos

    def _init_dds(self) -> None:
        self._logger.debug("Initialising DDS receiver resources (domain=%s topic=%s)",
                           self._dds_config.domain_id, self._dds_config.topic_name)
        dds_participant=DomainParticipant(self._dds_config.domain_id)
        topic = Topic(dds_participant, self._dds_config.topic_name, self._dds_config.topic_class)
        self._dds_reader = DataReader(Subscriber(dds_participant), topic, self._configure_datareader_qos())

    def read_samples(self):
        # TODO: use take_sample as the name of the method for consistency
        if self._dds_reader is None:
            self._logger.warning("DDS receiver not initialised, cannot read samples")
            return []

        samples = self._dds_reader.take()
        valid_samples = [sample.data for sample in samples if sample.info.valid]
        return valid_samples