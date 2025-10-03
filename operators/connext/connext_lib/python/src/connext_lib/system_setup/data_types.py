from rti.types import struct, key

@struct
class ReceiverResourceType:
    receiver_id: str = "", key
    buffer_id: str = ""


@struct()
class DummyUserType:
    id: int = 0, key
    message: str = ""