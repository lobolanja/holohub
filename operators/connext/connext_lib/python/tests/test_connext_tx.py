import time
from multiprocessing import shared_memory
from unittest import TestCase

from connext_lib.comm.connext_tx import ConnextTx
from connext_lib.endpoint_mng.dds_topic.dds_resources_managers import DDSSenderResourcesManager
from connext_lib.endpoint_mng.user_data.data_types import DummyUserType
from connext_lib.endpoint_mng.user_data.dds_disc_resources_managers import (
    DDSDiscSenderResourcesManager,
)
from connext_lib.payload_io.shmem_impl.mem_payload_io import MemPayloadReader, MemPayloadWriter


class TestConnextTx(TestCase):
    def test_broadcast(self):
        for BufferClass in [DDSSenderResourcesManager, DDSDiscSenderResourcesManager]:
            with self.subTest(buffer_class=BufferClass):
                if BufferClass is DDSDiscSenderResourcesManager:
                    adverted_buffer = BufferClass(
                        dds_domain_id=0,
                        user_topic_name="test_topic",
                        user_topic_type=DummyUserType
                    )
                else:
                    adverted_buffer = BufferClass(dds_domain_id=0)
                size = 16

                # setting up the writer shared memory buffer
                src_shm = shared_memory.SharedMemory("mem_tx", create=True, size=size)

                try:
                    # Setting the writer buffer with known data
                    test_data = b"hello world!!!"
                    src_shm.buf[: len(test_data)] = test_data

                    # the event name must be the same for writer and reader
                    event_name = "test_event"

                    # testing multiple readers
                    n_readers = 5

                    # here we will keep track of multiple reader shared memory buffers
                    dest_shm = {}
                    payload_reader = {}

                    for i in range(n_readers):
                        # setting up multiple reader buffers to use them to test the sut
                        dest_shm[i] = shared_memory.SharedMemory(f"mem_rx_{i+1}", create=True, size=size)
                        payload_reader[i] = MemPayloadReader(dest_shm[i].name, size, event_name)

                        # adding reader to the adverted buffer map so the sut knows about it
                        adverted_buffer._register_receiver(f"reader_{i+1}", dest_shm[i].name)

                    payload_writer = MemPayloadWriter(src_shm.name, size, event_name)

                    time.sleep(1)
                    sut = ConnextTx(adverted_buffer, payload_writer)

                    sut.broadcast_buffer()
                    time.sleep(0.1)

                    # verify that all readers received the data
                    for i in range(n_readers):
                        res = payload_reader[i].read_buffer()
                        self.assertTrue(res)
                        self.assertEqual(bytes(dest_shm[i].buf[: len(test_data)]), test_data)

                    # verify that no new data is available once it is read
                    for i in range(n_readers):
                        res = payload_reader[i].read_buffer()
                        self.assertFalse(res)
                finally:
                    src_shm.close()
                    src_shm.unlink()
                    if dest_shm is not None:
                        for shm in dest_shm.values():
                            shm.close()
                            shm.unlink()
            