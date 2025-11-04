import time
from unittest import TestCase

from connext_lib.endpoint_mng.user_data.data_types import DummyUserType
from connext_lib.endpoint_mng.user_data.dds_disc_resources_managers import (
    DDSDiscReceiverResourcesManager,
    DDSDiscSenderResourcesManager,
)
from rti.connextdds import DataWriter, DomainParticipant, Topic, UserData


class TestDDSDiscReceiverResourcesManager(TestCase):
    def test_announce_disc(self):
        expected_buffer_id = "disc-test-buffer"
        sut = DDSDiscReceiverResourcesManager(
            buffer_id=expected_buffer_id,
            user_topic_name="user_topic",
            user_topic_type=DummyUserType,
            dds_domain_id=0
        )

        # Use the built-in publication reader to check for the user data set by the subject under test
        dp = DomainParticipant(domain_id=0)
        reader = dp.publication_reader

        sut.announce()

        time.sleep(1)

        # Check that the user data was set correctly in the built-in publication topic
        samples = reader.take()
        found = False
        for sample in samples:
            if sample.info.valid:
                user_data_b = bytes(sample.data.user_data.value)
                user_data = user_data_b.decode("utf-8")
                if user_data == expected_buffer_id:
                    found = True
                    break
        self.assertTrue(found, f"User data {expected_buffer_id} not found in discovery info")

class TestDDSDiscSenderResourcesManager(TestCase):
    def test_register_disc(self):
        expected_buffer_id = "disc-test-buffer"
        sut = DDSDiscSenderResourcesManager(user_topic_name="user_topic", user_topic_type=DummyUserType ,dds_domain_id=0)

        # Create a DataWriter with the expected buffer ID in user data for testing purposes
        dp = DomainParticipant(domain_id=0)
        topic = Topic(dp, "system_setup", DummyUserType)
        info_bytes = f"{expected_buffer_id}".encode("utf-8")
        qos = dp.default_datawriter_qos
        qos.user_data = UserData(info_bytes)
        writer = DataWriter(dp.implicit_publisher, topic,qos)

        # Get the expected writer GUID for verification
        expected_writer_guid = str(writer.qos.protocol.virtual_guid)

        # Allow some time for discovery
        time.sleep(1)

        # Call register to process discovery info from remote endpoints (the DataWriter we just created)
        sut._register()

        # Check that the buffer was registered, the writer GUID is in the buffer map and the buffer ID matches
        self.assertTrue(hasattr(sut, "resources_map"), "DDSDiscSenderResourcesManager missing buffer_map")

        self.assertIn(expected_writer_guid, sut.resources_map, f"Writer GUID {expected_writer_guid} not registered")
        self.assertEqual(sut.resources_map[expected_writer_guid], expected_buffer_id, f"Buffer ID for writer GUID {expected_writer_guid} does not match expected {expected_buffer_id}")
