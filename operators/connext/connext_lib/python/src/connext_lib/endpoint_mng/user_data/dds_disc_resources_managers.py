import logging

from rti.connextdds import (
    DataReader,
    DataWriter,
    DomainParticipant,
    DomainParticipantQos,
    Duration,
    InstanceState,
    Topic,
    UserData,
)

from ..ifc.resources_managers import (
    AbstractSenderResourcesManager,
    ReceiverResourcesManagerInterface,
)


class DDSDiscReceiverResourcesManager(ReceiverResourcesManagerInterface):
    """
    Concrete implementation for ReceiverResourcesManagerInterface using DDS (Connext RTI)
    and the user data from the discovery info to notify the buffer_id to potential senders.
    """
    def __init__(self, buffer_id: str, user_topic_name: str, user_topic_type, dds_domain_id: int = 0):

        super().__init__()
        qos = DomainParticipantQos()
        # Configure discovery announcement periods to be very short to speed up detection
        # 10 ms = 0.01 sec
        qos.discovery_config.min_initial_participant_announcement_period = Duration(0, 100_000_000)  # 100 ms
        qos.discovery_config.max_initial_participant_announcement_period = Duration(0, 100_000_000)  # 100 ms
        qos.discovery_config.initial_participant_announcements = 30  # send 5 announcements

        # Configure liveliness to be more responsive
        qos.discovery_config.participant_liveliness_assert_period = Duration(5, 0)  # 5 sec
        qos.discovery_config.participant_liveliness_lease_duration = Duration(10, 0)  # 10 sec

        self._dp = DomainParticipant(domain_id=dds_domain_id, qos=qos)
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
        qos = DomainParticipantQos()
        # Configure discovery announcement periods to be very short to speed up detection
        # 10 ms = 0.01 sec
        qos.discovery_config.min_initial_participant_announcement_period = Duration(0, 100_000_000)  # 100 ms
        qos.discovery_config.max_initial_participant_announcement_period = Duration(0, 100_000_000)  # 100 ms
        qos.discovery_config.initial_participant_announcements = 30  # send 5 announcements

        # Configure liveliness to be more responsive
        qos.discovery_config.participant_liveliness_assert_period = Duration(5, 0)  # 5 sec
        qos.discovery_config.participant_liveliness_lease_duration = Duration(10, 0)  # 10 sec

        self._dp = DomainParticipant(domain_id=dds_domain_id, qos=qos)
        dds_topic = Topic(self._dp, user_topic_name, user_topic_type)
        self._dds_reader = DataReader(self._dp.implicit_subscriber, dds_topic)

        # this reader is used to access the built-in topics
        self._dds_pub_builtin_reader = self._dp.publication_reader
        self._logger = logging.getLogger(__name__)

    def __del__(self):
        super().__del__()
        if self._dds_reader:
            self._dds_reader.close()
        if self._dds_pub_builtin_reader:
            self._dds_pub_builtin_reader.close()
        if self._dp:
            self._dp.close()

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
                    reader_guid = str(sample.info.instance_handle)
                    self._logger.info("[DDSDiscSenderManager] Registering remote dw %s, user data for %s",reader_guid, user_data)
                    self._register_receiver(reader_guid, user_data)
                else:
                    # Check if the instance was unregistered or disposed
                    if sample.info.state.instance_state == InstanceState.NOT_ALIVE_NO_WRITERS or sample.info.state.instance_state == InstanceState.NOT_ALIVE_DISPOSED:
                        reader_guid = str(sample.info.instance_handle)
                        self._logger.info("[DDSDiscSenderManager] Unregistering remote receiver %s",reader_guid)
                        self._unregister_receiver(reader_guid)
        else:
            self._logger.error("[DDSDiscSenderManager] No DDS reader")
        return None

