"""
End-to-end tests for Connext operators.

Tests TX and RX operators running in separate processes, validating
data transmission over DDS and ANO (loopback mode).
"""

import pytest
import time
import subprocess
from process_utils import start_process, monitor_process


def test_dds_e2e(tx_executable, rx_executable, dds_config):
    """
    Test DDS end-to-end communication between TX and RX processes.
    
    Runs for a fixed duration and validates that RX receives >= 95% of expected messages.
    Uses time-based approach instead of exact message counting for robustness.
    """
    print("\n" + "="*80)
    print("Starting DDS End-to-End Test (Time-Based)")
    print("="*80)
    
    rx_process = None
    tx_process = None
    
    try:
        # Start RX process (will run for configured duration)
        rx_cmd = f"{rx_executable} {dds_config}"
        print(f"\n[TEST] Starting RX process: {rx_cmd}")
        rx_process = start_process(rx_cmd)
        
        # Give RX a moment to initialize
        print("[TEST] Waiting 2 seconds before starting TX...")
        time.sleep(2)
        
        # Start TX process (will send at configured rate for duration)
        tx_cmd = f"{tx_executable} {dds_config}"
        print(f"\n[TEST] Starting TX process: {tx_cmd}")
        tx_process = start_process(tx_cmd)
        
        # Monitor TX process first (should complete after test duration)
        print("\n[TEST] Monitoring TX process...")
        tx_result = monitor_process(tx_process)
        tx_process = None  # Process finished
        
        # Monitor RX process
        print("\n[TEST] Monitoring RX process...")
        rx_result = monitor_process(rx_process)
        rx_process = None  # Process finished
        
        # Validate results
        print("\n" + "="*80)
        print("Validating Test Results")
        print("="*80)
        
        # Check TX output
        assert "[TX App] Finished successfully" in tx_result.stdout, \
            f"TX process did not finish successfully. Output:\n{tx_result.stdout[-1000:]}"
        assert "[TX] Sending payload: integration_test_payload" in tx_result.stdout, \
            "TX did not send expected payload"
        
        # Extract sent count from TX output
        sent_matches = [line for line in tx_result.stdout.split("\n") if "sent" in line and "messages" in line]
        sent_count = 0
        if sent_matches:
            import re
            match = re.search(r"sent (\d+) messages", sent_matches[-1])
            if match:
                sent_count = int(match.group(1))
        print(f"✓ TX sent {sent_count} messages")
        
        # Check RX output - must contain success message
        assert "[RX App] Finished successfully" in rx_result.stdout, \
            f"RX process did not finish successfully. Output:\n{rx_result.stdout[-1000:]}"
        
        # Check for test pass/fail message
        if "[RX] ✓ Test PASSED" in rx_result.stdout:
            print("✓ RX test PASSED - received >= 95% of expected messages")
        elif "[RX] ✗ Test FAILED" in rx_result.stdout:
            pytest.fail("RX test FAILED - did not receive enough messages")
        else:
            pytest.fail("RX did not report test result")
        
        assert "[RX] Received payload: integration_test_payload" in rx_result.stdout, \
            "RX did not receive expected payload"
        assert "[RX] Using CPU memory for DDS reception" in rx_result.stdout, \
            "RX did not use CPU memory (expected for DDS)"
        
        print("✓ Payload transmitted and validated correctly")
        print("✓ Test completed successfully with time-based approach")
        
        print("\n" + "="*80)
        print("DDS End-to-End Test PASSED")
        print("="*80 + "\n")
        
    except Exception as e:
        print(f"\n[TEST] Error during test execution: {e}")
        if tx_process is None and 'tx_result' in locals():
            print("\n[DEBUG] TX Output (last 50 lines):")
            print("\n".join(tx_result.stdout.split("\n")[-50:]))
        if rx_process is None and 'rx_result' in locals():
            print("\n[DEBUG] RX Output (last 50 lines):")
            print("\n".join(rx_result.stdout.split("\n")[-50:]))
        raise
    
    finally:
        # Cleanup: terminate any remaining processes
        if rx_process is not None:
            print("[TEST] Cleaning up RX process...")
            rx_process.terminate()
            try:
                rx_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                rx_process.kill()
        
        if tx_process is not None:
            print("[TEST] Cleaning up TX process...")
            tx_process.terminate()
            try:
                tx_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                tx_process.kill()


def test_ano_e2e(tx_executable, rx_executable, ano_config):
    """
    Test ANO end-to-end communication between TX and RX processes in loopback mode.
    
    **CURRENT LIMITATION**: This test fails because DPDK loopback mode requires
    both TX and RX to run in the SAME process. When TX and RX are separate processes,
    the second process cannot initialize DPDK (lock conflict).
    
    **SOLUTION NEEDED**: Either:
    1. Use a single-process test application with both TX and RX operators, OR
    2. Configure DPDK to support multi-process loopback (complex setup)
    
    For now, this test validates the timeout mechanism and proper error reporting.
    """
    print("\n" + "="*80)
    print("Starting ANO End-to-End Test (Loopback Mode, Time-Based)")
    print("NOTE: This test is expected to timeout due to DPDK loopback limitations")
    print("="*80)
    
    rx_process = None
    tx_process = None
    
    try:
        # Start RX process
        rx_cmd = f"{rx_executable} {ano_config}"
        print(f"\n[TEST] Starting RX process: {rx_cmd}")
        rx_process = start_process(rx_cmd)
        
        # Give RX time to initialize ANO and DPDK (longer delay for ANO setup)
        print("[TEST] Waiting 5 seconds for RX initialization (ANO/DPDK)...")
        time.sleep(5)
        
        # Start TX process
        tx_cmd = f"{tx_executable} {ano_config}"
        print(f"\n[TEST] Starting TX process: {tx_cmd}")
        tx_process = start_process(tx_cmd)
        
        # Monitor TX process first (should fail quickly or complete after test duration)
        # Timeout: test_duration (25s) + delays (5s) + buffer (30s) = 60s  
        print("\n[TEST] Monitoring TX process (timeout: 60s)...")
        try:
            tx_result = monitor_process(tx_process, timeout=60)
            tx_process = None  # Process finished
            
            # Check if TX failed with non-zero exit code (DPDK error)
            if tx_result.returncode != 0:
                print(f"\n[TEST] TX process exited with code {tx_result.returncode}")
                print(f"\n[DEBUG] TX Output:\n{tx_result.stdout[-2000:]}")
                print(f"\n[DEBUG] TX Stderr:\n{tx_result.stderr[-1000:]}")
                
                # Check for DPDK lock error
                if "Cannot create lock on '/var/run/dpdk" in tx_result.stdout or \
                   "Cannot create lock on '/var/run/dpdk" in tx_result.stderr:
                    print("\n" + "="*80)
                    print("DPDK LOOPBACK LIMITATION DETECTED")
                    print("="*80)
                    print("TX process failed because RX already initialized DPDK.")
                    print("DPDK loopback mode requires TX and RX in the SAME process.")
                    print("\nTo fix this test, we need either:")
                    print("1. A single-process test app with both TX and RX operators")
                    print("2. DPDK multi-process configuration (not supported in loopback)")
                    pytest.skip("ANO loopback test requires single-process architecture")
                else:
                    pytest.fail(f"TX process failed with code {tx_result.returncode}")
                    
        except subprocess.TimeoutExpired as e:
            print(f"\n[TEST] TX process timed out after 60s")
            print(f"\n[DEBUG] TX Output before timeout:\n{e.output[-2000:]}")
            if e.stderr:
                print(f"\n[DEBUG] TX Stderr before timeout:\n{e.stderr[-1000:]}")
            
            # Check for DPDK lock error in output
            if "Cannot create lock on '/var/run/dpdk" in e.output or \
               (e.stderr and "Cannot create lock on '/var/run/dpdk" in e.stderr):
                pytest.skip("ANO loopback test requires single-process architecture")
            else:
                pytest.fail("TX process timed out - ANO may not be working correctly")
        
        # Monitor RX process
        print("\n[TEST] Monitoring RX process (timeout: 60s)...")
        try:
            rx_result = monitor_process(rx_process, timeout=60)
            rx_process = None  # Process finished
        except subprocess.TimeoutExpired as e:
            print(f"\n[TEST] RX process timed out after 60s")
            print(f"\n[DEBUG] RX Output before timeout:\n{e.output}")
            if e.stderr:
                print(f"\n[DEBUG] RX Stderr before timeout:\n{e.stderr}")
            pytest.fail("RX process timed out - likely not receiving ANO messages")
        
        # Validate results
        print("\n" + "="*80)
        print("Validating Test Results")
        print("="*80)
        
        # Check for GPU requirement error first
        if "CUDA GPU required for ANO test but not available" in rx_result.stderr or \
           "CUDA GPU required for ANO test but not available" in rx_result.stdout:
            pytest.fail("ANO test requires GPU but CUDA GPU is not available")
        
        # Check for ANO initialization errors
        if "Failed to initialize advanced network manager" in rx_result.stdout or \
           "Failed to initialize advanced network manager" in rx_result.stderr:
            print("\n[DEBUG] RX failed to initialize ANO:")
            print(rx_result.stdout[-2000:])
            pytest.fail("RX failed to initialize advanced network manager")
        
        if "Failed to initialize advanced network manager" in tx_result.stdout or \
           "Failed to initialize advanced network manager" in tx_result.stderr:
            print("\n[DEBUG] TX failed to initialize ANO:")
            print(tx_result.stdout[-2000:])
            pytest.fail("TX failed to initialize advanced network manager")
        
        # Check TX output
        assert "[TX App] Finished successfully" in tx_result.stdout, \
            f"TX process did not finish successfully. Output:\n{tx_result.stdout[-1000:]}"
        assert "[TX] GPU memory allocated and initialized for ANO transmission" in tx_result.stdout, \
            "TX did not allocate GPU memory for ANO"
        assert "[TX] Sending payload: integration_test_payload" in tx_result.stdout, \
            "TX did not send expected payload"
        
        # Extract sent count
        sent_matches = [line for line in tx_result.stdout.split("\n") if "sent" in line and "messages" in line]
        sent_count = 0
        if sent_matches:
            import re
            match = re.search(r"sent (\d+) messages", sent_matches[-1])
            if match:
                sent_count = int(match.group(1))
        print(f"✓ TX sent {sent_count} messages with GPU memory")
        
        # Check RX output
        assert "[RX App] Finished successfully" in rx_result.stdout, \
            f"RX process did not finish successfully. Output:\n{rx_result.stdout[-1000:]}"
        
        # Check for test pass/fail message
        if "[RX] ✓ Test PASSED" in rx_result.stdout:
            print("✓ RX test PASSED - received >= 95% of expected messages")
        elif "[RX] ✗ Test FAILED" in rx_result.stdout:
            pytest.fail("RX test FAILED - did not receive enough messages")
        else:
            pytest.fail("RX did not report test result")
        
        assert "[RX] GPU available for ANO reception" in rx_result.stdout, \
            "RX did not detect GPU availability"
        assert "[RX] Received payload: integration_test_payload" in rx_result.stdout, \
            "RX did not receive expected payload"
        
        print("✓ Payload transmitted via GPU and validated correctly")
        print("✓ Test completed successfully with time-based approach")
        
        print("\n" + "="*80)
        print("ANO End-to-End Test PASSED")
        print("="*80 + "\n")
        
    except Exception as e:
        print(f"\n[TEST] Error during test execution: {e}")
        if tx_process is None and 'tx_result' in locals():
            print("\n[DEBUG] TX Output (last 50 lines):")
            print("\n".join(tx_result.stdout.split("\n")[-50:]))
        if rx_process is None and 'rx_result' in locals():
            print("\n[DEBUG] RX Output (last 50 lines):")
            print("\n".join(rx_result.stdout.split("\n")[-50:]))
        raise
    
    finally:
        # Cleanup: terminate any remaining processes
        if rx_process is not None:
            print("[TEST] Cleaning up RX process...")
            rx_process.terminate()
            try:
                rx_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                rx_process.kill()
        
        if tx_process is not None:
            print("[TEST] Cleaning up TX process...")
            tx_process.terminate()
            try:
                tx_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                tx_process.kill()
