from rti.types import key, struct


@struct
class ReceiverResourceType:
    receiver_id: str = "", key
    buffer_id: str = ""
