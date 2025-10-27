import logging
from typing import Any

from rti.connextdds import (
    DomainParticipant,
    Topic,
    DataWriter,
    DataReader,
    DataReaderQos,
    DataWriterQos,
    ReliabilityKind,
    DurabilityKind,
    HistoryKind,
)
from .data_types import ReceiverResourceType
from .resources_managers import (
    ReceiverResourcesManagerInterface,
    AbstractSenderResourcesManager,
)

class DDSReceiverResourcesManager(ReceiverResourcesManagerInterface):
    """
    Concrete implementation for ReceiverResourcesManagerInterface using DDS (Connext RTI)
    and a topic for publishing the buffer_id to potential senders.
    """
    def __init__(self, buffer_id : str, dds_domain_id: int = 0, topic_name: str = "system_setup"):
        super().__init__()
        dp = DomainParticipant(domain_id=dds_domain_id)
        dds_topic = Topic(dp, topic_name, ReceiverResourceType)
        self._buffer_id = buffer_id

        writer_qos = DataWriterQos()
        writer_qos.reliability.kind = ReliabilityKind.RELIABLE
        writer_qos.history.kind = HistoryKind.KEEP_ALL
        writer_qos.durability.kind = DurabilityKind.TRANSIENT_LOCAL
        self._dds_writer = DataWriter(dp.implicit_publisher, dds_topic, writer_qos)

        self._logger = logging.getLogger(__name__)
    def __del__(self):
        if self._dds_writer:
            self._dds_writer.close()
        # Note: DomainParticipant will be closed when the program ends

    def _get_writer_guid(self) -> str | Any:
        """ Get the GUID of the DataWriter """
        protocol = self._dds_writer.qos.protocol
        writer_guid = str(protocol.virtual_guid)
        return writer_guid

    def announce(self):
        self._logger.info("[DDSReceiverResourcesManager] Announcing buffer %s via DDS.", self._buffer_id)
        if self._dds_writer:

            writer_guid = self._get_writer_guid()

            message = ReceiverResourceType(receiver_id=writer_guid, buffer_id=self._buffer_id)
            self._logger.info("[DDSReceiverResourcesManager] Sending message %s", message)
            self._dds_writer.write(message)
        else:
            self._logger.error("[DDSReceiverResourcesManager] No DDS writer")

class DDSSenderResourcesManager(AbstractSenderResourcesManager):
    """
    Concrete implementation for AbstractSenderResourcesManager using DDS (Connext RTI)
    and a topic for receiving the buffer_id from potential receivers.
    """
    def __init__(self, dds_domain_id: int = 0, topic_name: str = "system_setup"):
        super().__init__()
        dp = DomainParticipant(domain_id=dds_domain_id)
        dds_topic = Topic(dp, topic_name, ReceiverResourceType)

        # Create DataReader QoS and set strict reliable policies
        reader_qos = DataReaderQos()
        reader_qos.reliability.kind = ReliabilityKind.RELIABLE
        reader_qos.history.kind = HistoryKind.KEEP_ALL
        reader_qos.durability.kind = DurabilityKind.TRANSIENT_LOCAL

        self._dds_reader = DataReader(dp.implicit_subscriber, dds_topic, reader_qos)

        self._logger = logging.getLogger(__name__)
    def __del__(self):
        super().__del__()
        if self._dds_reader:
            self._dds_reader.close()
        # Note: DomainParticipant will be closed when the program ends

    def _register(self):
        """
        Read from the DDS topic and register new receivers.
        """
        if self._dds_reader:
            data = self._dds_reader.take()
            for sample in data:
                if sample.info.valid:
                    self._register_receiver(sample.data.receiver_id, sample.data.buffer_id)
                    self._logger.info("[DDSReceiverResourcesManager] registering receiver %s for buffer %s",sample.data.receiver_id, sample.data.buffer_id)
                else:
                    receiver_id = str(sample.info.original_publication_virtual_guid)
                    self._unregister_receiver(receiver_id)
                    self._logger.info("[DDSReceiverResourcesManager] unregistering receiver %s",receiver_id)
        else:
            self._logger.error("[DDSSenderResourcesManager] No DDS reader")
        return None

