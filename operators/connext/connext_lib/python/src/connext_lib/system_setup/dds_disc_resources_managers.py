import logging

from rti.connextdds import DomainParticipant, Topic, DataWriter, DataReader, UserData
from .resources_managers import ReceiverResourcesManagerInterface, AbstractSenderResourcesManager


class DDSDiscReceiverResourcesManager(ReceiverResourcesManagerInterface):
    """
    Concrete implementation for ReceiverResourcesManagerInterface using DDS (Connext RTI)
    and the user data from the discovery info to notify the buffer_id to potential senders.
    """
    def __init__(self, buffer_id: str, user_topic_name: str, user_topic_type, dds_domain_id: int = 0):

        super().__init__()
        self._dp = DomainParticipant(domain_id=dds_domain_id)
        self._dds_topic = Topic(self._dp, user_topic_name, user_topic_type)
        self._buffer_id = buffer_id
        self._dds_writer = None

        self._logger = logging.getLogger(__name__)

    def __del__(self):
        if self._dds_writer:
            self._dds_writer.close()
        # Note: DomainParticipant will be closed when the program ends


    def _populate_datawriter_with_discovery_info(self):
        info_bytes = f"{self._buffer_id}".encode("utf-8")
        qos = self._dp.default_datawriter_qos
        qos.user_data = UserData(info_bytes)
        self._dds_writer = DataWriter(self._dp.implicit_publisher, self._dds_topic, qos)


    def announce(self):
        self._logger.info("[DDSDiscReceiverResourcesManager] Announcing buffer %s via Discovery.", self._buffer_id)
        self._populate_datawriter_with_discovery_info()
        if self._dds_writer is None:
            self._logger.error("[DDSDiscReceiverResourcesManager] No DDS writer")


class DDSDiscSenderResourcesManager(AbstractSenderResourcesManager):
    """
    Concrete implementation for AbstractSenderResourcesManager using DDS (Connext RTI)
    and the user data from the discovery info to register potential receivers.
    """
    def __init__(self, user_topic_name: str, user_topic_type, dds_domain_id: int = 0):
        super().__init__()
        dp = DomainParticipant(domain_id=dds_domain_id)
        dds_topic = Topic(dp, user_topic_name, user_topic_type)
        self._dds_reader = DataReader(dp.implicit_subscriber, dds_topic)

        # this reader is used to access the built-in topics
        self._dds_pub_builtin_reader = dp.publication_reader
        self._logger = logging.getLogger(__name__)

    def __del__(self):
        super().__del__()
        if self._dds_reader:
            self._dds_reader.close()
        if self._dds_pub_builtin_reader:
            self._dds_pub_builtin_reader.close()
        # Note: DomainParticipant will be closed when the program ends

    def _register(self):
        """
        Check the built-in topic for new receivers(dds writers) and register them if they have user data.
        """
        if self._dds_reader:
            samples = self._dds_pub_builtin_reader.take()
            for sample in samples:
                if sample.info.valid:
                    # Extract user data and writer GUID from the sample
                    user_data_bytes = bytes(sample.data.user_data.value)
                    user_data = user_data_bytes.decode("utf-8")
                    writer_guid = str(sample.data.virtual_guid)
                    self._logger.info("[DDSDiscSenderManager] Registering remote dw %s, user data for %s",writer_guid, user_data)
                    self._register_receiver(writer_guid, user_data)
        else:
            self._logger.error("[DDSDiscSenderManager] No DDS reader")
        return None

