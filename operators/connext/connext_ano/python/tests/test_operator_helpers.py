from holoscan.core import Operator, OperatorSpec


class BufferSourceOp(Operator):
    """Simple Holoscan source that emits a fixed payload once."""

    def __init__(self, fragment, *args, payload: str, **kwargs):
        super().__init__(fragment, *args, **kwargs)
        self._payload = payload
        self._emitted_pkgs = 0

    def setup(self, spec: OperatorSpec):
        spec.output("output")

    def start(self):  # pragma: no cover - deterministic initialisation
        super().start()
        self._emitted_pkgs = 0

    def compute(self, _op_input, op_output, _context):
        self._emitted_pkgs += 1
        self._payload = self._payload + f"_#{self._emitted_pkgs}"
        op_output.emit(self._payload, "output")


class BufferSinkOp(Operator):
    """Sink operator that records received payloads."""

    def __init__(self, fragment, *, storage: list[str], **kwargs):
        super().__init__(fragment, **kwargs)
        self._storage = storage

    def setup(self, spec: OperatorSpec):
        spec.input("input")

    def compute(self, op_input, _op_output, _context):
        payload = op_input.receive("input")
        if payload is not None:
            self._storage.append(payload)


class DDSSinkOp(Operator):
    """Sink operator that records received payloads."""

    def __init__(self, fragment, *, storage: list[str], **kwargs):
        super().__init__(fragment, **kwargs)
        self._storage = storage

    def setup(self, spec: OperatorSpec):
        spec.input("input")

    def compute(self, op_input, _op_output, _context):
        payload = op_input.receive("input")
        if payload is not None:
            for item in payload:
                self._storage.append(str(item.data))
