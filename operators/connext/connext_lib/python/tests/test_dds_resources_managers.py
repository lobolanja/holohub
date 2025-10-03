import time
from unittest import TestCase

from rti.connextdds import (DataReader, DataWriter, DomainParticipant,
                            Subscriber, Topic, DataWriterQos, ReliabilityKind, HistoryKind, DurabilityKind)

from connext_lib.system_setup.dds_resources_managers import DDSSenderResourcesManager, DDSReceiverResourcesManager
from connext_lib.system_setup.data_types import ReceiverResourceType

class TestDDSReceiverResourcesManager(TestCase):
    def test_announce(self):
        expected_buffer_id = "test-buffer"
        sut = DDSReceiverResourcesManager(
            buffer_id=expected_buffer_id, dds_domain_id=0, topic_name="system_setup"
        )
        # create a dds reader to get the message
        dp = DomainParticipant(domain_id=0)
        topic = Topic(dp, "system_setup", ReceiverResourceType)
        subscriber = Subscriber(dp)
        reader = DataReader(subscriber, topic)

        time.sleep(1)  # wait for DDS to set up

        sut.announce()

        # sleep briefly to allow DDS to process (in real test, use synchronization)
        time.sleep(0.1)
        samples = reader.take()
        assert samples, "No samples received from DDS"
        found = any(sample.data.buffer_id == expected_buffer_id for sample in samples)
        assert found, f"Buffer ID {expected_buffer_id} not found in DDS samples"


class TestDDSSenderResourcesManager(TestCase):
    def test_register(self):

        expected_buffer_id = "test-buffer"
        sut = DDSSenderResourcesManager(dds_domain_id=0)
        sut.start_processing()

        # create a dds writer to send the message
        dp = DomainParticipant(domain_id=0)
        topic = Topic(dp, "system_setup", ReceiverResourceType)
        publisher = dp.implicit_publisher

        writer_qos = DataWriterQos()
        writer_qos.reliability.kind = ReliabilityKind.RELIABLE
        writer_qos.history.kind = HistoryKind.KEEP_ALL
        writer_qos.durability.kind = DurabilityKind.TRANSIENT_LOCAL

        writer = DataWriter(publisher, topic, writer_qos)

        time.sleep(1)
        # send a message
        writer_guid = "10"
        message = ReceiverResourceType(
            receiver_id=writer_guid, buffer_id=expected_buffer_id
        )
        writer.write(message)

        # sleep briefly to allow DDS to process (in real test, use synchronization)
        time.sleep(0.1)

        sut.stop_processing()

        assert (
            writer_guid in sut.resources_map
        ), f"Writer GUID {writer_guid} not registered"
        assert (
            sut.resources_map[writer_guid] == expected_buffer_id
        ), f"Buffer ID for writer GUID {writer_guid} does not match expected {expected_buffer_id}"


class TestDDSResourcesManagersIntegration(TestCase):
    def test_integration(self):
        expected_buffer_id = "integration-buffer"
        advertiser = DDSReceiverResourcesManager(expected_buffer_id)
        register = DDSSenderResourcesManager(dds_domain_id=0)
        register.start_processing()

        time.sleep(1)  # wait for DDS to set up

        advertiser.announce()

        # sleep briefly to allow DDS to process (in real test, use synchronization)
        time.sleep(0.1)

        register.stop_processing()

        writer_guid = advertiser._get_writer_guid()
        assert (
            writer_guid in register.resources_map
        ), f"Writer GUID {writer_guid} not registered in integration test"
        assert (
            register.resources_map[writer_guid] == expected_buffer_id
        ), f"Buffer ID for writer GUID {writer_guid} does not match expected {expected_buffer_id} in integration test"
