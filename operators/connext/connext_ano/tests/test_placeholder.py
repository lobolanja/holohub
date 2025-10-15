import pytest

from connext_ano import ConnextAnoRxOp, ConnextAnoTxOp, DDSConfig, ANOConfig


def test_tx_constructs_with_defaults():
    op = ConnextAnoTxOp(fragment=None, name="tx", shm_name="test", shm_size=1024)
    assert op is not None


def test_rx_constructs_with_defaults():
    op = ConnextAnoRxOp(fragment=None, name="rx", shm_name="test", shm_size=1024)
    assert op is not None


def test_transport_state_toggle():
    tx = ConnextAnoTxOp(
        fragment=None,
        name="tx",
        shm_name="test",
        shm_size=256,
        ano_config=ANOConfig(enabled=True),
        dds_config=DDSConfig()
    )
    tx._transport_state.activate_ano()
    assert tx._transport_state.should_use_ano() is True
