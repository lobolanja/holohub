import logging

from ..system_setup.resources_managers import ReceiverResourcesManagerInterface
from ..payload_io.abstract_payload_io import PayloadReaderInterface

class ConnextRx:
    def __init__(self, receiver_resources_mgr: ReceiverResourcesManagerInterface, payload_reader: PayloadReaderInterface):
        """
        receiver_resources_mgr: An instance of ReceiverResourcesManagerInterface to announce itself.
        payload_reader: An instance of PayloadReaderInterface to read from shared memory.
        """
        self._receiver_resources_mgr = receiver_resources_mgr
        self._payload_reader = payload_reader

        # Announce the buffer to potential senders
        self._receiver_resources_mgr.announce()

        self._logger = logging.getLogger(__name__)

    def __del__(self):
        pass

    def receive_buffer(self):
        """
        Read data from the buffer.
        return true if new data is in the reader buffer, false otherwise.
        """
        result = self._payload_reader.read_buffer()

        return result