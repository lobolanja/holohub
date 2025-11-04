import fcntl
import logging
from multiprocessing import shared_memory

from ..ifc.abstract_payload_io import PayloadReaderInterface, PayloadWriterInterface
from .interprocess_events import Event, EventSubscriber


class _MemFileLock:
    """ Private file lock used by MemPayloadWriter and MempayloadReader for file-based locking using fcntl."""
    def __init__(self, path="/tmp/mempayloadiobus.lock"):
        self._path = path
        self._fd = None

    def __enter__(self):
        self._fd = open(self._path, "w")
        fcntl.flock(self._fd, fcntl.LOCK_EX)

    def __exit__(self, exc_type, exc_val, exc_tb):
        fcntl.flock(self._fd, fcntl.LOCK_UN)
        self._fd.close()

class MemPayloadWriter(PayloadWriterInterface):
    """MemPayloadWriter Class writes payloads to shared memory and notifies readers via an event."""
    def __init__(self, name: str, size: int, event_name: str):
        self._name = name
        self._size = size
        self._event = Event(event_name)

        self._logger = logging.getLogger(__name__)

    def set_buffer(self, payload: bytes | bytearray | memoryview | str) -> None:
        """Set the data to be written to shared memory, accepting text or binary payloads."""
        if isinstance(payload, str):
            data = payload.encode("utf-8")
        else:
            data = bytes(payload)

        if len(data) > self._size:
            raise ValueError(
                f"Payload of {len(data)} bytes exceeds shared-memory segment size {self._size}"
            )

        writer_shm = shared_memory.SharedMemory(name=self._name)
        with _MemFileLock():
            writer_shm.buf[:len(data)] = data
            if len(data) < self._size:
                writer_shm.buf[len(data):self._size] = b"\x00" * (self._size - len(data))


    def write_buffer(self, reader_reference):
        """ Writes data from the writer's shared memory to the reader's shared memory and notifies the reader."""
        try:
            writer_shm = shared_memory.SharedMemory(name=self._name)
            reader_shm = shared_memory.SharedMemory(name=reader_reference)
            with _MemFileLock():
                reader_shm.buf[:self._size] = writer_shm.buf[:self._size]

            message = bytes(writer_shm.buf[:self._size])
            self._logger.info("[Writer] Writing data to shared memory: %s, %s",  reader_reference, message.decode("utf-8", errors="replace"))
            self._event.notify()
        except FileNotFoundError:
            self._logger.info("[Reader] Shared memory %s or %s not found.", self._name, reader_reference)
            return

class MemPayloadReader(PayloadReaderInterface):
    """MemPayloadReader Class reads payloads from shared memory when notified by an event."""
    def __init__(self, name, size, event_name: str):
        self._name = name
        self._size = size
        self._event = EventSubscriber(event_name, name)

        self._logger = logging.getLogger(__name__)

    def read_buffer(self):
        """ Reads data from shared memory when notified by the event. """
        if not self._event.wait(timeout=1):  # Wait for new data 1 second
            self._logger.info("[Reader] No new data in shared memory %s.", self._name)
            return False
        try:
            with _MemFileLock():
                shm = shared_memory.SharedMemory(name=self._name)
                message = bytes(shm.buf[:])
                self._logger.info("[Reader] Shared memory %s has %s.", self._name, message.decode("utf-8", errors="replace"))
        except FileNotFoundError:
            self._logger.info("[Reader] Shared memory %s not found.", self._name)
            return False
        return True
