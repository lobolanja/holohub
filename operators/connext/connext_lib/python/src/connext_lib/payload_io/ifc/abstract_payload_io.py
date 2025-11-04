from abc import ABC, abstractmethod


class PayloadWriterInterface(ABC):
    #TODO: think better names for the methods
    """
    Interface for payload writer classes.

    Classes implementing this interface must provide a method to write data
    to a destination.

    Methods
    -------
    set_buffer(payload: bytes) -> None
    write_buffer(reference)
        Write data to the given destination.
    """
    @abstractmethod
    def set_buffer(self, payload: bytes) -> None:
        """
        Set the data to be written to shared memory.

        :param payload: The payload data to write.
        :type payload: bytes
        :return: None
        """
        pass

    @abstractmethod
    def write_buffer(self, destination_reference):
        """
        Write data to the provided destination.

        :param destination_reference: Reference to the destination to write data into.
        :type destination_reference: Any
        :return: None
        """
        pass


class PayloadReaderInterface(ABC):
    """
    Interface for payload reader classes.

    Classes implementing this interface must provide a method to read data
    from an internal buffer, returning True if new data is available, False otherwise.

    Methods
    -------
    read_buffer()
        Read data from the internal buffer.
    """

    @abstractmethod
    def read_buffer(self):
        """
        Read data from the internal buffer.

        :return: True if new data is available, False otherwise.
        """
        pass
