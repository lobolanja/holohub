from rti.types import key, struct


@struct
class DummyUserType:
    id: int = 0, key
    message: str = ""