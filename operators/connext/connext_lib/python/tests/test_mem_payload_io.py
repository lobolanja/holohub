from multiprocessing import shared_memory
from unittest import TestCase

from connext_lib.payload_io.shmem_impl.interprocess_events import Event
from connext_lib.payload_io.shmem_impl.mem_payload_io import MemPayloadReader, MemPayloadWriter


class TestMemPayloadWriter(TestCase):
    def test_write(self):
        reader_shm_name = "test_shm_reader"
        writer_shm_name = "test_shm_writer"
        size = 10
        shm_reader = shared_memory.SharedMemory(
            name=reader_shm_name, create=True, size=size
        )
        shm_writer = shared_memory.SharedMemory(
            name=writer_shm_name, create=True, size=size
        )
        try:
            # Write some data to shared memory
            shm_writer.buf[:5] = b"hello"
            sut = MemPayloadWriter(writer_shm_name, size, "test_event")
            # Should print the written data
            sut.write_buffer(reader_shm_name)
            # Check the buffer content
            self.assertEqual(bytes(shm_reader.buf[:5]), b"hello")
        finally:
            shm_writer.close()
            shm_writer.unlink()
            shm_reader.close()
            shm_reader.unlink()


class TestMemPayloadReader(TestCase):
    def test_read(self):
        name = "test_shm"
        size = 10
        shm = shared_memory.SharedMemory(name=name, create=True, size=size)
        try:
            shm.buf[:5] = b"world"
            event = Event("test_event")
            sut = MemPayloadReader(shm.name, size, "test_event")
            event.notify()
            result = sut.read_buffer()
            self.assertTrue(result)
            self.assertEqual(bytes(shm.buf[:5]), b"world")
        finally:
            shm.close()
            shm.unlink()
