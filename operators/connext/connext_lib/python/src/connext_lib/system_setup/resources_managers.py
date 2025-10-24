import logging
import threading
import time
from abc import ABC, abstractmethod

__all__ = (
    "ReceiverResourcesManagerInterface",
    "SenderResourcesManagerInterface",
    "AbstractSenderResourcesManager",
)

class ReceiverResourcesManagerInterface(ABC):
    """
    An interface for buffer reference sharing with remote senders.
    """

    def __init__(self):
        pass

    @abstractmethod
    def announce(self):
        pass


class SenderResourcesManagerInterface(ABC):
    """
    An interface for buffer reference registration from remote receivers.
    """
    def __init__(self):
        pass

    @abstractmethod
    def start_processing(self, poll_interval=0.1):
        pass
    @abstractmethod
    def stop_processing(self):
        pass
    @abstractmethod
    def _register(self):
        pass
    @abstractmethod
    def get_destinations(self):
        pass

class AbstractSenderResourcesManager(SenderResourcesManagerInterface):
    """
    An abstract class for buffer reference registration from remote endpoints.
    """

    def __init__(self):
        super().__init__()
        self._thread = None
        self._stop_event = threading.Event()
        self._lock = threading.Lock()
        self.resources_map = {}  # destination -> buffer_id

        self._logger = logging.getLogger(__name__)
    def __del__(self):
        self.stop_processing()
        self.resources_map.clear()

    def _register_receiver(self, destination, buffer_id):
        """" Register a receiver's buffer ID if not already registered."""
        with self._lock:
            if destination not in self.resources_map:
                self.resources_map[destination] = buffer_id
                self._logger.info("[AbstractSenderResourcesManager] Registered buffer with ID %s from destination %s", buffer_id, destination)
            else:
                self._logger.debug("[AbstractSenderResourcesManager] Buffer from destination %s already registered", destination)


    def _process(self, poll_interval):
        """ Background thread method to periodically call _register()."""
        while not self._stop_event.is_set():
            self._register()
            time.sleep(poll_interval)

    @abstractmethod
    def _register(self):
        """ This method has to be implemented by subclasses to process incoming receiver resources."""
        pass

    def start_processing(self, poll_interval=0.1):
        """ Start the background thread to process incoming registrations."""
        self._thread = threading.Thread(target=self._process, args=(poll_interval,))
        self._thread.daemon = True
        self._thread.start()
        self._logger.info("[AbstractSenderResourcesManager] Started background DDS sample processing thread.")

    def stop_processing(self):
        """ Stop the background thread."""
        if hasattr(self, '_stop_event'):
            self._stop_event.set()
            self._thread.join()
            self._logger.info("[AbstractSenderResourcesManager] Stopped background DDS sample processing thread.")
    def get_destinations(self):
        """ Get a thread-safe copy of the current resources map."""
        with self._lock:
            copy = dict(self.resources_map)
        return copy




