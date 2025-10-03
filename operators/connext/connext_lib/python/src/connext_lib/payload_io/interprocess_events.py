# EventSubscription manages individual subscriber status
import pickle
from multiprocessing import shared_memory, Lock


import fcntl

class _EvenBusFileLock:
    """A simple file-based lock for synchronizing access to Even Bus shared memory.
    This class is private to the EventBus implementation."""
    def __init__(self, path="/tmp/eventbus.lock"):
        self._path = path
        self._fd = None

    def __enter__(self):
        self._fd = open(self._path, "w")
        fcntl.flock(self._fd, fcntl.LOCK_EX)

    def __exit__(self, exc_type, exc_val, exc_tb):
        fcntl.flock(self._fd, fcntl.LOCK_UN)
        self._fd.close()

class _EventBus:
    """A simple event bus using shared memory for inter-process communication.
    The event bus maintains a mapping of event names to subscriber IDs and their notification status.
    This class is private to the Event and EventSubscriber implementations."""

    _shm_name = "eventbus_shm"
    _shm_size = 10**6  # 1 MB of shared memory

    @classmethod
    def _init_memory(cls):
        try:
            # create shared memory
            cls._shm = shared_memory.SharedMemory(
                name=cls._shm_name, create=True, size=cls._shm_size
            )
            # init to empty dict
            empty = pickle.dumps({})
            cls._shm.buf[: len(empty)] = empty
        except FileExistsError:
            # the shared memory already exists
            cls._shm = shared_memory.SharedMemory(name=cls._shm_name, create=False)

    @classmethod
    def _read_map(cls):
        cls._init_memory()
        raw = bytes(cls._shm.buf).rstrip(b"\x00")
        if not raw:
            return {}
        return pickle.loads(raw)

    @classmethod
    def _write_map(cls, data):
        cls._init_memory()
        raw = pickle.dumps(data)
        if len(raw) > cls._shm_size:
            raise MemoryError("Shared memory size too small")
        cls._shm.buf[: len(raw)] = raw
        # Zero out the rest of the buffer if needed
        remaining = cls._shm_size - len(raw)
        if remaining > 0:
            cls._shm.buf[len(raw):len(raw)+remaining] = b"\x00" * remaining

    @classmethod
    def register_subscriber(cls, event_name: str, subscriber_id: str):
        """Register a subscriber to an event. Initializes its notification status to 0 (not notified)."""
        with _EvenBusFileLock():
            data = cls._read_map()
            if event_name not in data:
                data[event_name] = {}
            data[event_name][subscriber_id] = 0
            cls._write_map(data)

    @classmethod
    def notify(cls, event_name: str):
        """Notify all subscribers of an event by setting their status to 1 (notified)."""
        with _EvenBusFileLock():
            data = cls._read_map()
            if event_name in data:
                for k in data[event_name].keys():
                    data[event_name][k] = 1
                cls._write_map(data)

    @classmethod
    def consume(cls, event_name: str, subscriber_id: str) -> bool:
        """Consume the notification for a subscriber. Returns True if the event was notified since the last consume, False otherwise."""
        with _EvenBusFileLock():
            data = cls._read_map()
            if event_name in data and data[event_name].get(subscriber_id, 0) == 1:
                data[event_name][subscriber_id] = 0
                cls._write_map(data)
                return True
            return False


class Event:
    """Event class to notify subscribers via EventBus."""
    def __init__(self, event_name: str):
        self.name = event_name

    def notify(self):
        """Notify all subscribers of this event."""
        _EventBus.notify(self.name)


class EventSubscriber:
    """EventSubscriber class to subscribe to events via EventBus."""
    def __init__(self, event_name: str, subscriber_id: str):
        self.event_name = event_name
        self.subscriber_id = subscriber_id
        _EventBus.register_subscriber(event_name, subscriber_id)

    def poll(self):
        """Returns True if the event was notified since the last poll, False otherwise."""
        return _EventBus.consume(self.event_name, self.subscriber_id)

    def wait(self, timeout:float):
        """Waits until the event is notified or timeout occurs. Returns True if notified, False if timeout."""
        import time

        start_time = time.time() # start time in seconds (float with microsecond precision)
        while True:
            if self.poll():
                return True
            if timeout is not None and (time.time() - start_time) >= timeout:
                return False
            time.sleep(0.01)  # Sleep briefly to avoid busy waiting
