import logging

from ..system_setup.resources_managers import SenderResourcesManagerInterface
from ..payload_io.abstract_payload_io import PayloadWriterInterface

class ConnextTx:
    def __init__(self, sender_resources_mgr : SenderResourcesManagerInterface, payload_writer : PayloadWriterInterface):
        """
        sender_resources_mgr: to manage the destination's buffer_ids
        payload_writer: to write to the destination buffers.
        """
        self._sender_resources_mgr = sender_resources_mgr
        self._payload_writer = payload_writer

        # Start the sender resources manager processing thread to listen for new registrations
        self._sender_resources_mgr.start_processing()

        self._logger = logging.getLogger(__name__)


    def __del__(self):
        if hasattr(self, '_sender_resources_mgr'):
            # Stop the sender resources manager processing thread
            self._sender_resources_mgr.stop_processing()

    def broadcast_buffer(self, src_buffer_ref):
        """
        Write the contents of src_data_ref to all registered buffers.
        """

        for destination, dest_buffer_ref in self._sender_resources_mgr.get_destinations().items():
            try:
                self._payload_writer.write_buffer(dest_buffer_ref)
                self._logger.info("Wrote data from %s to %s (destination id: %s)", src_buffer_ref, src_buffer_ref, destination)
            except FileNotFoundError:
                self._logger.warning("Destination shared memory %s not found for destination id %s.", src_buffer_ref, destination)
                continue
