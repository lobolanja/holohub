import time
from time import sleep

from connext_lib.cfg import ANOConfig, DDSConfig
from connext_ano.connext_op_tx import ConnextAnoTxOp
from connext_ano.connext_op_rx import ConnextAnoRxOp
from holoscan.core import Application
from test_application_helpers import ConnextApplicationANODummy, ConnextApplicationDDSDummy

def test_tx_constructs_with_defaults():
    app = Application()
    op = ConnextAnoTxOp(fragment=app)
    assert op is not None


def test_rx_constructs_with_defaults():
    app = Application()
    op = ConnextAnoRxOp(fragment=app)
    assert op is not None


def test_tx_starts_and_stops():
    app = Application()
    tx = ConnextAnoTxOp(
        fragment=app,
        ano_config=ANOConfig(enabled=True),
        dds_config=DDSConfig(),
    )
    tx.start()
    tx.stop()
    assert True  # If no exceptions, the test passes


def test_rx_starts_and_stops():
    app = Application()
    rx = ConnextAnoRxOp(
        fragment=app,
        ano_config=ANOConfig(enabled=True),
        dds_config=DDSConfig(),
    )
    rx.start()
    rx.stop()
    assert True  # If no exceptions, the test passes

def test_tx_rx_discovery():

    rx_shm_name = "rx_shm_memory"

    app = Application()
    tx = ConnextAnoTxOp(
        fragment=app,
        ano_config=ANOConfig(shm_name="tx_shm_memory", enabled=True),
        dds_config=DDSConfig(),
    )
    rx = ConnextAnoRxOp(
        fragment=app,
        ano_config=ANOConfig(shm_name=rx_shm_name, enabled=True),
        dds_config=DDSConfig(),
    )

    tx.start()
    rx.start()
    sleep(2)  # Allow some time for discovery to complete

    # Check if both operators have finished the discovery process
    assert rx_shm_name in tx.connext_ano_writer.get_discovery_manager().get_destinations().values()
    tx.stop()
    rx.stop()
    sleep(2)
    assert True

def test_tx_rx_ano_integration():
    rx_shm_name = "rx_shm_memory"
    payload = "hello_holoscan"
    timeout = 25 # seconds

    app_tx = ConnextApplicationANODummy(rx_shm_name=rx_shm_name, payload=payload)

    print("Running TX application...")
    app_tx.run()

    print(f"Waiting for registration during {timeout} seconds max")
    start_time = time.time()
    while not app_tx._tx_op.connext_ano_writer.get_discovery_manager().get_destinations():
        time.sleep(0.1)
        if time.time() - start_time > timeout:
            print ("Timeout waiting for the Sender to register the Receiver")
            assert False
    time.sleep(0.1)
    # Assert payload string in one of received string list
    received_payloads = app_tx.received
    print("Received payloads:", received_payloads)
    assert any(payload.encode("utf-8") in received for received in received_payloads)

def test_tx_rx_dds_integration():
    payload = "hello_holoscan"
    app_tx = ConnextApplicationDDSDummy(payload=payload)
    print("Running TX application...")
    app_tx.run()
    # Assert payload string in one of received string list
    received_payloads = app_tx.received
    print("Received payloads:", received_payloads)
    assert any(payload in received for received in received_payloads)


