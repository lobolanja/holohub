import logging
import pytest
import os
from subprocess import CalledProcessError
from process_utils import monitor_process, run_command, start_process

# Configure the logger
logger = logging.getLogger(__name__)

def test_demo_ano_tx_rx_app():
    """
    Test the demo_ano_tx_rx application with basic parameters.
    This test starts the application, monitors its output, and ensures it runs correctly.
    The application is terminated via timeout, so we only verify output, not exit code.
    """
    #holohub_root = os.environ.get('HOLOHUB_ROOT', '/workspace/holohub')
    #cmd = f"{holohub_root}/holohub run demo_ano_tx_rx demo_ano_tx_rx_loopback_test.yaml"
    cmd = "timeout 5 ./demo_ano_tx_rx tests/demo_ano_tx_rx_loopback_test.yaml"
    
    # Start the process
    process = start_process(cmd)
    
    try:
        # Monitor the process (will raise CalledProcessError due to timeout exit code 124)
        try:
            result = monitor_process(process)
        except CalledProcessError as e:
            # Expected: timeout returns exit code 124
            # We only care about the output, not the exit code
            logger.info(f"Process terminated with exit code {e.returncode} (expected from timeout)")
            result = e
        
        # Check for specific output in stdout to verify TX and RX worked
        output = result.stdout if hasattr(result, 'stdout') else ""
        assert "Received packet" in output, "Expected RX output 'Received packet' not found in stdout"
        
        logger.info("✓ Test passed: TX sent packets and RX received them successfully")
        
    finally:
        # Ensure the process is terminated
        process.terminate()
        process.wait()

# TODO: Not so easy to run TX and RX in different containers due to DPDK conflicts.
# def test_demo_ano_tx_rx_diff_processes():
#     """
#     Test the demo_ano application with TX and RX running in different processes/containers.
#     This test uses the holohub run command which runs each process in its own container,
#     avoiding DPDK conflicts. The RX process starts first, then the TX process.
#     Both processes are terminated via timeout.
#     """
#     import time
    
#     holohub_root = os.environ.get('HOLOHUB_ROOT', '/workspace/holohub')
        
#     # Shared docker and build options for both TX and RX containers
#     docker_opts = "--docker-opts=\"-v ./rti_license.dat:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat --cap-add=SYS_ADMIN --cap-add=IPC_LOCK --cap-add=NET_ADMIN --device=/dev/hugepages:/dev/hugepages --ulimit memlock=-1:-1 --privileged -v /dev/hugepages:/dev/hugepages\""
#     configure_args = "--configure-args=\"-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0\""
#     common_opts = f"{configure_args}"
    
#     # Use holohub run command to run each process in separate containers
#     cmd_rx = f"timeout 40 {holohub_root}/holohub run connext_ano_basic_app {common_opts} --run-args='rx /workspace/holohub/applications/connext/connext_ano_basic_app/tests/demo_ano_rx_test.yaml'"
#     cmd_tx = f"timeout 40 {holohub_root}/holohub run connext_ano_basic_app {common_opts} --run-args='tx /workspace/holohub/applications/connext/connext_ano_basic_app/tests/demo_ano_tx_test.yaml'"
    
#     # Start the RX process first
#     # Start the RX process first
#     logger.info("Starting RX process in container...")
#     process_rx = start_process(cmd_rx)
    
#     # Give RX time to initialize and container to start
#     time.sleep(3)
    
#     # Start the TX process
#     logger.info("Starting TX process in container...")
#     process_tx = start_process(cmd_tx)
    
#     try:
#         # Monitor both processes (will raise CalledProcessError due to timeout exit code 124)
#         rx_result = None
#         tx_result = None
        
#         try:
#             tx_result = monitor_process(process_tx)
#         except CalledProcessError as e:
#             # Expected: timeout returns exit code 124
#             logger.info(f"TX process terminated with exit code {e.returncode} (expected from timeout)")
#             tx_result = e
        
#         try:
#             rx_result = monitor_process(process_rx)
#         except CalledProcessError as e:
#             # Expected: timeout returns exit code 124
#             logger.info(f"RX process terminated with exit code {e.returncode} (expected from timeout)")
#             rx_result = e
        
#         # Check for specific output in stdout to verify TX sent and RX received
#         tx_output = tx_result.stdout if hasattr(tx_result, 'stdout') else ""
#         rx_output = rx_result.stdout if hasattr(rx_result, 'stdout') else ""
        
#         # Verify RX received packets
#         assert "Received packet" in rx_output, "Expected RX output 'Received packet' not found in stdout"
        
#         logger.info("✓ Test passed: TX and RX processes communicated successfully")
        
#     finally:
#         # Ensure both processes are terminated
#         process_tx.terminate()
#         process_rx.terminate()
#         process_tx.wait()
#         process_rx.wait()


    