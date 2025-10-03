# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

import sys
import types
from pathlib import Path
from uuid import uuid4

# ---------------------------------------------------------------------------
# Lightweight stubs for holoscan.core so the operators can be instantiated
# without requiring the full Holoscan runtime during unit testing.
# ---------------------------------------------------------------------------
if "holoscan.core" not in sys.modules:
    holoscan_mod = types.ModuleType("holoscan")
    core_mod = types.ModuleType("holoscan.core")

    class _Fragment:  # pragma: no cover - placeholder for constructor signature
        pass

    class _Operator:
        def __init__(self, fragment=None, *_, **__):  # pragma: no cover - noop init
            self.fragment = fragment

        def stop(self):  # pragma: no cover - noop stop hook
            pass

    class _OperatorSpec:  # pragma: no cover - contract placeholder
        def input(self, *_args, **_kwargs):
            pass

        def output(self, *_args, **_kwargs):
            pass

    core_mod.Fragment = _Fragment
    core_mod.Operator = _Operator
    core_mod.OperatorSpec = _OperatorSpec
    holoscan_mod.core = core_mod
    sys.modules["holoscan"] = holoscan_mod
    sys.modules["holoscan.core"] = core_mod

# ---------------------------------------------------------------------------
# Stubs for holohub.connext_lib used by the operators under test.
# ---------------------------------------------------------------------------
holohub_pkg = sys.modules.setdefault("holohub", types.ModuleType("holohub"))
connext_lib_pkg = types.ModuleType("holohub.connext_lib")

# Submodules: comm, payload_io, system_setup
comm_mod = types.ModuleType("holohub.connext_lib.comm")
payload_mod = types.ModuleType("holohub.connext_lib.payload_io")
system_mod = types.ModuleType("holohub.connext_lib.system_setup")


class _StubSenderManager:
    def __init__(self, *args, **kwargs):  # pragma: no cover - simple recorder
        self.args = args
        self.kwargs = kwargs
        self.started = False

    def start_processing(self):
        self.started = True

    def stop_processing(self):  # pragma: no cover - noop
        self.started = False

    def get_destinations(self):  # pragma: no cover - not used in tests
        return {}


class _StubReceiverManager:
    def __init__(self, *args, **kwargs):  # pragma: no cover - simple recorder
        self.args = args
        self.kwargs = kwargs
        self.announced = False

    def announce(self):
        self.announced = True


class _StubConnextTx:
    def __init__(self, sender_mgr, payload_writer):
        self.sender_mgr = sender_mgr
        self.payload_writer = payload_writer
        self.broadcasts = []
        self.sender_mgr.start_processing()

    def broadcast_buffer(self, shm_name):
        self.broadcasts.append(shm_name)


class _StubConnextRx:
    def __init__(self, receiver_mgr, payload_reader):
        self.receiver_mgr = receiver_mgr
        self.payload_reader = payload_reader
        self.receiver_mgr.announce()
        self.should_receive = False

    def receive_buffer(self):
        result = self.should_receive
        self.should_receive = False
        return result


class _StubPayloadWriter:  # pragma: no cover - argument recorder only
    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs


class _StubPayloadReader:  # pragma: no cover - argument recorder only
    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs


class _DummyUserType:  # pragma: no cover - metadata placeholder
    pass


comm_mod.ConnextTx = _StubConnextTx
comm_mod.ConnextRx = _StubConnextRx
payload_mod.MemPayloadWriter = _StubPayloadWriter
payload_mod.MemPayloadReader = _StubPayloadReader
system_mod.DDSDiscSenderResourcesManager = _StubSenderManager
system_mod.DDSSenderResourcesManager = _StubSenderManager
system_mod.DDSDiscReceiverResourcesManager = _StubReceiverManager
system_mod.DDSReceiverResourcesManager = _StubReceiverManager
system_mod.DummyUserType = _DummyUserType

connext_lib_pkg.comm = comm_mod
connext_lib_pkg.payload_io = payload_mod
connext_lib_pkg.system_setup = system_mod

sys.modules["holohub.connext_lib"] = connext_lib_pkg
sys.modules["holohub.connext_lib.comm"] = comm_mod
sys.modules["holohub.connext_lib.payload_io"] = payload_mod
sys.modules["holohub.connext_lib.system_setup"] = system_mod

# ---------------------------------------------------------------------------
# Add paths for the operator packages and import the modules under test.
# ---------------------------------------------------------------------------
this_dir = Path(__file__).resolve().parent
sys.path.append(str(this_dir.parent / "connext_op_tx" / "python"))
sys.path.append(str(this_dir.parent / "connext_op_rx" / "python"))

from connext_op_tx import ConnextTxOp  # noqa: E402  (import after path tweaks)
from connext_op_rx import ConnextRxOp  # noqa: E402


def _unique_name(prefix):
    return f"{prefix}_{uuid4().hex}"


class _DummyInput:
    def __init__(self, payload):
        self._payload = payload

    def receive(self, port_name):
        assert port_name == "payload"
        return self._payload


class _DummyOutput:
    def __init__(self):
        self.emitted = []

    def emit(self, value, port_name, data_type=None):
        self.emitted.append((value, port_name, data_type))


def test_connext_tx_op_compute_writes_and_broadcasts():
    shm_name = _unique_name("test_tx")
    op = ConnextTxOp(
        fragment=None,
        name="tx",
        shm_name=shm_name,
        shm_size=16,
        zero_pad=True,
    )
    try:
        payload = b"abcde"
        op.compute(_DummyInput(payload), None, None)

        assert op._tx.broadcasts == [shm_name]
        buf = bytes(op._shm.buf[:16])
        assert buf[: len(payload)] == payload
        assert all(b == 0 for b in buf[len(payload) :])
    finally:
        op._shutdown()


def test_connext_tx_op_supports_discovery_manager():
    shm_name = _unique_name("test_tx_disc")
    op = ConnextTxOp(
        fragment=None,
        name="tx_disc",
        shm_name=shm_name,
        shm_size=8,
        use_discovery=True,
        discovery_topic_name="custom_topic",
    )
    try:
        assert isinstance(op._sender_mgr, _StubSenderManager)
        assert op._sender_mgr.kwargs["user_topic_name"] == "custom_topic"
    finally:
        op._shutdown()


def test_connext_rx_op_emits_bytes_when_data_available():
    shm_name = _unique_name("test_rx")
    payload = b"hello world"
    op = ConnextRxOp(
        fragment=None,
        name="rx",
        shm_name=shm_name,
        shm_size=len(payload),
    )
    try:
        op._rx.should_receive = True
        op._shm.buf[: len(payload)] = payload

        output = _DummyOutput()
        op.compute(None, output, None)

        assert output.emitted == [(payload, "payload", None)]
    finally:
        op._shutdown()


def test_connext_rx_op_can_emit_buffer_name():
    shm_name = _unique_name("test_rx_name")
    op = ConnextRxOp(
        fragment=None,
        name="rx_name",
        shm_name=shm_name,
        shm_size=4,
        output_mode="buffer_name",
        use_discovery=True,
        discovery_topic_name="topic_name",
    )
    try:
        op._rx.should_receive = True

        output = _DummyOutput()
        op.compute(None, output, None)

        assert output.emitted == [(shm_name, "payload", "std::string")]
        assert isinstance(op._receiver_mgr, _StubReceiverManager)
        assert op._receiver_mgr.kwargs["user_topic_name"] == "topic_name"
    finally:
        op._shutdown()


def test_connext_rx_op_no_emit_when_receive_returns_false():
    shm_name = _unique_name("test_rx_none")
    op = ConnextRxOp(
        fragment=None,
        name="rx_none",
        shm_name=shm_name,
        shm_size=8,
    )
    try:
        output = _DummyOutput()
        op.compute(None, output, None)

        assert output.emitted == []
    finally:
        op._shutdown()
