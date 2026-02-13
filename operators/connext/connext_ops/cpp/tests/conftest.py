"""
Pytest configuration and fixtures for Connext operators end-to-end tests.
"""

import pytest
import os
import subprocess


def pytest_addoption(parser):
    """Add custom command line options for pytest."""
    parser.addoption(
        "--workdir",
        action="store",
        default=os.getcwd(),
        help="Working directory containing test executables and configs"
    )


@pytest.fixture(scope="module")
def work_dir(request):
    """Get the working directory containing test executables."""
    return request.config.getoption("--workdir")


@pytest.fixture(scope="module")
def tx_executable(work_dir):
    """Get path to TX application executable."""
    exe_path = os.path.join(work_dir, "e2e_tx_app")
    if not os.path.exists(exe_path):
        pytest.fail(f"TX executable not found: {exe_path}")
    return exe_path


@pytest.fixture(scope="module")
def rx_executable(work_dir):
    """Get path to RX application executable."""
    exe_path = os.path.join(work_dir, "e2e_rx_app")
    if not os.path.exists(exe_path):
        pytest.fail(f"RX executable not found: {exe_path}")
    return exe_path


@pytest.fixture(scope="module")
def dds_config(work_dir):
    """Get path to DDS configuration file."""
    config_path = os.path.join(work_dir, "e2e_config_dds.yaml")
    if not os.path.exists(config_path):
        pytest.fail(f"DDS config not found: {config_path}")
    return config_path


@pytest.fixture(scope="module")
def ano_config(work_dir):
    """Get path to ANO configuration file."""
    config_path = os.path.join(work_dir, "e2e_config_ano.yaml")
    if not os.path.exists(config_path):
        pytest.fail(f"ANO config not found: {config_path}")
    return config_path


@pytest.fixture(scope="session")
def gpu_available():
    """
    Check if CUDA GPU is available for ANO tests.
    
    Returns:
        bool: True if CUDA GPU is available, False otherwise
    """
    try:
        result = subprocess.run(
            ["nvidia-smi"],
            capture_output=True,
            timeout=5,
            text=True
        )
        is_available = result.returncode == 0
        if is_available:
            print("\n[FIXTURE] GPU detected via nvidia-smi")
        else:
            print("\n[FIXTURE] No GPU detected (nvidia-smi failed)")
        return is_available
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        print(f"\n[FIXTURE] GPU check failed: {e}")
        return False
