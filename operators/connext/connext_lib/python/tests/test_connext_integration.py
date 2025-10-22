import time
from multiprocessing import shared_memory
from unittest import TestCase

from connext_lib.system_setup.data_types import DummyUserType
from connext_lib.system_setup.dds_resources_managers import DDSSenderResourcesManager, DDSReceiverResourcesManager
from connext_lib.system_setup.dds_disc_resources_managers import DDSDiscSenderResourcesManager, DDSDiscReceiverResourcesManager
from connext_lib.comm.connext_tx import ConnextTx
from connext_lib.comm.connext_rx import ConnextRx
from connext_lib.payload_io.mem_payload_io import MemPayloadReader, MemPayloadWriter

class TestConnextIntegration(TestCase):
    def test_tx_rx_integration(self):
        # Test both DDS and DDSDisc resource managers
        for BufferClass in [DDSSenderResourcesManager, DDSDiscSenderResourcesManager]:
            with self.subTest(buffer_class=BufferClass):
                if BufferClass is DDSDiscSenderResourcesManager:
                    sender_resources_mgr = DDSDiscSenderResourcesManager(
                        dds_domain_id=0,
                        user_topic_name="test_topic",
                        user_topic_type=DummyUserType
                    )
                    receiver_resources_mgr = DDSDiscReceiverResourcesManager(
                        dds_domain_id=0,
                        buffer_id="mem_rx",
                        user_topic_name="test_topic",
                        user_topic_type=DummyUserType
                    )
                else:
                    sender_resources_mgr = DDSSenderResourcesManager()
                    receiver_resources_mgr = DDSReceiverResourcesManager(buffer_id="mem_rx")
                # Create shared memory segments for the payload_writer and payload_reader
                size = 16
                shm_tx = shared_memory.SharedMemory("mem_tx", create=True, size=size)
                shm_rx = shared_memory.SharedMemory("mem_rx", create=True, size=size)
                try:
                    test_data = b"hello world!!!"
                    shm_tx.buf[:len(test_data)] = test_data

                    # Create payload writer and reader. They are in charge of writing/reading to/from shared memory
                    event_name = "test_event"
                    payload_writer = MemPayloadWriter(shm_tx.name, size, event_name)
                    payload_reader = MemPayloadReader(shm_rx.name, size, event_name)

                    # Create ConnextTx and ConnextRx instances, they are the subject under test
                    tx = ConnextTx(sender_resources_mgr, payload_writer)
                    rx = ConnextRx(receiver_resources_mgr, payload_reader)


                    timeout = 5 # seconds
                    print(f"Waiting for registration during {timeout} seconds max")
                    start_time = time.time()
                    while not sender_resources_mgr.get_destinations():
                        if time.time() - start_time > timeout:
                            self.fail("Timeout waiting for the Sender to register the Receiver")
                        time.sleep(0.1)

                    print("Starting broadcast")
                    tx.broadcast_buffer()
                    time.sleep(0.1)

                    # Now the receiver should have the data in its shared memory
                    result = rx.receive_buffer()
                    self.assertTrue(result)
                    self.assertEqual(bytes(shm_rx.buf[:len(test_data)]), test_data)
                finally:
                    shm_tx.close()
                    shm_tx.unlink()
                    shm_rx.close()
                    shm_rx.unlink()
                    time.sleep(1)