import pytest

from connext_ano import ConnextAnoRxOp, ConnextAnoTxOp, DDSConfig, ANOConfig
from holoscan.core import Application

def test_tx_constructs_with_defaults():
    app = Application()
    op = ConnextAnoTxOp(fragment=app, name="tx", shm_name="test", shm_size=1024)
    assert op is not None


def test_rx_constructs_with_defaults():
    app = Application()
    op = ConnextAnoRxOp(fragment=app, name="rx", shm_name="test", shm_size=1024)
    assert op is not None


def test_transport_state_toggle():
    app = Application()
    tx = ConnextAnoTxOp(
        fragment=app,
        name="tx",
        shm_name="test",
        shm_size=256,
        ano_config=ANOConfig(enabled=True),
        dds_config=DDSConfig()
    )
    tx._transport_state.activate_ano()
    assert tx._transport_state.should_use_ano() is True
