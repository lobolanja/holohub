import time
from multiprocessing import shared_memory
from unittest import TestCase

from connext_lib.system_setup.data_types import DummyUserType
from connext_lib.system_setup.dds_resources_managers import DDSReceiverResourcesManager
from connext_lib.system_setup.dds_disc_resources_managers import DDSDiscReceiverResourcesManager
from connext_lib.comm.connext_rx import ConnextRx
from connext_lib.payload_io.mem_payload_io import MemPayloadReader, MemPayloadWriter


class TestConnextRx(TestCase):
    def test_receive(self):
        for BufferClass in [DDSReceiverResourcesManager, DDSDiscReceiverResourcesManager]:
            with self.subTest(buffer_class=BufferClass):
                if BufferClass is DDSDiscReceiverResourcesManager:
                    advert_buffer = BufferClass(
                        dds_domain_id=0,
                        buffer_id="my_buffer_id",
                        user_topic_name="test_topic",
                        user_topic_type=DummyUserType
                    )
                else:
                    advert_buffer = BufferClass(
                        dds_domain_id=0,
                        buffer_id="my_buffer_id",
                        topic_name="test_topic")
                size = 16
                # Create two shared memory blocks, one for Connext TX and one for Connext RX
                shm_rx = shared_memory.SharedMemory("mem_rx", create=True, size=size)
                shm_tx = shared_memory.SharedMemory("mem_tx", create=True, size=size)
                try:
                    # Event name must match between reader and writer
                    event_name = "test_event"

                    # set the tx shared memory buffer to known data
                    test_data = b"hello world!!!"
                    shm_tx.buf[:len(test_data)] = test_data

                    # this writer is a helper for writing data to the rx buffer
                    writer = MemPayloadWriter(shm_tx.name, size, event_name)

                    # setting up the SUT Connext RX with the rx shared memory and the advert buffer
                    payload_reader = MemPayloadReader(shm_rx.name, size, event_name)
                    sut = ConnextRx(advert_buffer, payload_reader)

                    writer.write_buffer(shm_rx.name)
                    time.sleep(0.1)
                    result = sut.receive_buffer()
                    self.assertTrue(result)
                    # After reading once, buffer should be empty
                    result = sut.receive_buffer()
                    self.assertFalse(result)
                finally:
                    shm_rx.close()
                    shm_rx.unlink()
                    shm_tx.close()
                    shm_tx.unlink()