#!/usr/bin/env python3
"""
Shared utilities for Connext end-to-end tests

This module provides common infrastructure for ANO and DDS end-to-end container tests,
eliminating code duplication and following SOLID principles.
"""

import os
import re
import subprocess
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path
from typing import Tuple, List, Optional


# =============================================================================
# Print Utilities
# =============================================================================

def print_header(msg: str) -> None:
    """Print test section header with border"""
    print(f"\n{'='*80}")
    print(f"{msg}")
    print(f"{'='*80}\n")


def print_success(msg: str) -> None:
    """Print success message with checkmark"""
    print(f"✓ {msg}")


def print_error(msg: str) -> None:
    """Print error message with cross"""
    print(f"✗ {msg}")


# =============================================================================
# Data Classes
# =============================================================================

@dataclass
class TestScriptPaths:
    """Encapsulates test script file paths"""
    rx_script: Path
    tx_script: Path
    rx_config: Path
    tx_config: Path
    
    @classmethod
    def for_ano(cls) -> 'TestScriptPaths':
        """Factory for ANO test paths"""
        script_dir = Path(__file__).parent
        return cls(
            rx_script=script_dir / "scripts" / "run_e2e_rx_container.sh",
            tx_script=script_dir / "scripts" / "run_e2e_tx_container.sh",
            rx_config=script_dir / "config" / "test_ano_rx.yaml",
            tx_config=script_dir / "config" / "test_ano_tx.yaml"
        )
    
    @classmethod
    def for_dds(cls) -> 'TestScriptPaths':
        """Factory for DDS test paths"""
        script_dir = Path(__file__).parent
        return cls(
            rx_script=script_dir / "scripts" / "run_e2e_dds_rx_container.sh",
            tx_script=script_dir / "scripts" / "run_e2e_dds_tx_container.sh",
            rx_config=script_dir / "config" / "test_dds_rx.yaml",
            tx_config=script_dir / "config" / "test_dds_tx.yaml"
        )
    
    def validate_exist(self) -> List[str]:
        """Return list of missing files"""
        missing = []
        for attr in ['rx_script', 'tx_script', 'rx_config', 'tx_config']:
            path = getattr(self, attr)
            if not path.exists():
                missing.append(str(path))
        return missing


@dataclass
class TestParameters:
    """Test execution parameters"""
    timeout_seconds: int = 90
    success_threshold: float = 0.80
    rx_init_wait: int = 5
    message_rate: int = 10


@dataclass
class TestResults:
    """Encapsulates parsed test results"""
    tx_sent: int
    rx_received: int
    tx_output: str
    rx_output: str
    
    @property
    def success_rate(self) -> float:
        """Calculate success rate percentage"""
        if self.tx_sent == 0:
            return 0.0
        return (self.rx_received / self.tx_sent) * 100.0
    
    def meets_threshold(self, threshold: float) -> bool:
        """Check if results meet success threshold"""
        if self.tx_sent == 0:
            return False
        min_required = int(self.tx_sent * threshold)
        return self.rx_received >= min_required


# =============================================================================
# Environment Validation
# =============================================================================

class EnvironmentValidator(ABC):
    """Base validator for test environment"""
    
    def __init__(self, paths: TestScriptPaths):
        self.paths = paths
    
    def validate(self) -> bool:
        """Run all validation checks"""
        print_header("Validating Environment")
        
        # Common: RTI license
        if not self._validate_rti_license():
            return False
        
        # Common: Scripts
        if not self._validate_scripts():
            return False
        
        # Transport-specific
        return self._validate_transport_specific()
    
    def _validate_rti_license(self) -> bool:
        """Validate RTI license file"""
        rti_license = os.getenv("RTI_LICENSE_FILE", "./rti_license.dat")
        if not os.path.exists(rti_license):
            print_error(f"RTI license file not found: {rti_license}")
            return False
        print_success(f"RTI License: {rti_license}")
        return True
    
    def _validate_scripts(self) -> bool:
        """Validate container scripts and config files"""
        missing = self.paths.validate_exist()
        if missing:
            for path in missing:
                print_error(f"File not found: {path}")
            return False
        print_success("Container scripts and config files found")
        return True
    
    @abstractmethod
    def _validate_transport_specific(self) -> bool:
        """Validate transport-specific requirements"""
        pass


class ANOEnvironmentValidator(EnvironmentValidator):
    """Validator for ANO-specific environment"""
    
    def _validate_transport_specific(self) -> bool:
        """Validate ANO requires NIC PCIe addresses"""
        tx_nic = os.getenv("TEST_TX_NIC_PCIE")
        rx_nic = os.getenv("TEST_RX_NIC_PCIE")
        
        if not tx_nic:
            print_error("TEST_TX_NIC_PCIE environment variable not set")
            return False
        if not rx_nic:
            print_error("TEST_RX_NIC_PCIE environment variable not set")
            return False
        
        print_success(f"TX NIC PCIe: {tx_nic}")
        print_success(f"RX NIC PCIe: {rx_nic}")
        return True


class DDSEnvironmentValidator(EnvironmentValidator):
    """Validator for DDS-specific environment"""
    
    def _validate_transport_specific(self) -> bool:
        """DDS has no additional requirements beyond common checks"""
        print_success("DDS environment ready")
        return True


# =============================================================================
# Output Parsing
# =============================================================================

class OutputParser(ABC):
    """Base parser for container outputs"""
    
    @abstractmethod
    def get_payload_pattern(self) -> str:
        """Return regex pattern for payload messages"""
        pass
    
    def parse(self, tx_output: str, rx_output: str) -> TestResults:
        """Parse TX and RX outputs to extract results"""
        # Extract TX sent count
        pattern = self.get_payload_pattern()
        tx_matches = re.findall(f'PayloadSource sending payload: {pattern}', tx_output)
        
        if not tx_matches:
            print_error("Could not find any 'PayloadSource sending payload' messages in TX output")
            tx_sent = 0
        else:
            tx_sent = len(tx_matches)
            print_success(f"TX sent: {tx_sent} messages (last message #{tx_matches[-1]})")
        
        # Extract RX received count
        rx_matches = re.findall(f'PayloadSink received payload: {pattern}', rx_output)
        rx_received = self._parse_rx_received(rx_matches, rx_output, tx_output)
        
        return TestResults(tx_sent, rx_received, tx_output, rx_output)
    
    @abstractmethod
    def _parse_rx_received(self, rx_matches: List[str], rx_output: str, tx_output: str) -> int:
        """Parse RX received count with transport-specific fallbacks"""
        pass
    
    def _try_summary_pattern(self, rx_output: str) -> int:
        """Try to extract count from summary pattern"""
        match = re.search(r'Receiver collected (\d+) payload\(s\)', rx_output)
        if match:
            count = int(match.group(1))
            print_success(f"RX received: {count} messages (from summary)")
            return count
        print_error("Test ran but couldn't verify reception")
        return 0


class ANOOutputParser(OutputParser):
    """Parser for ANO transport outputs"""
    
    def get_payload_pattern(self) -> str:
        return r'integration_test_payload_#(\d+)'
    
    def _parse_rx_received(self, rx_matches: List[str], rx_output: str, tx_output: str) -> int:
        """Parse with ANO-specific fallbacks"""
        if rx_matches:
            count = len(rx_matches)
            print_success(f"RX received: {count} messages (last message #{rx_matches[-1]})")
            return count
        
        # Fallback: Summary
        return self._try_summary_pattern(rx_output)


class DDSOutputParser(OutputParser):
    """Parser for DDS transport outputs"""
    
    def get_payload_pattern(self) -> str:
        return r'integration_test_dds_payload_#(\d+)'
    
    def _parse_rx_received(self, rx_matches: List[str], rx_output: str, tx_output: str) -> int:
        """Parse with DDS-specific fallbacks (simpler than ANO)"""
        if rx_matches:
            count = len(rx_matches)
            print_success(f"RX received: {count} messages (last message #{rx_matches[-1]})")
            return count
        
        # Fallback: Summary only (no ANO confirmations)
        return self._try_summary_pattern(rx_output)


# =============================================================================
# Container Orchestration
# =============================================================================

class ContainerOrchestrator:
    """Manages container lifecycle for end-to-end tests"""
    
    def __init__(self, paths: TestScriptPaths, params: TestParameters):
        self.paths = paths
        self.params = params
    
    def run_containers(self) -> Tuple[str, str]:
        """Run TX and RX containers, return (tx_output, rx_output)"""
        print_header(f"Starting Test (timeout={self.params.timeout_seconds}s)")
        
        # Start RX
        rx_process = self._start_rx_container()
        
        # Wait for RX initialization
        print(f"Waiting {self.params.rx_init_wait} seconds for RX initialization...")
        time.sleep(self.params.rx_init_wait)
        
        # Start TX
        tx_process = self._start_tx_container()
        
        # Run test duration
        print(f"Running test for {self.params.timeout_seconds} seconds...")
        print("(Containers will be terminated after timeout)\n")
        time.sleep(self.params.timeout_seconds)
        
        # Stop containers
        self._stop_containers([rx_process, tx_process])
        
        # Collect outputs
        return self._collect_outputs(tx_process, rx_process)
    
    def _start_rx_container(self) -> subprocess.Popen:
        """Start RX container in background"""
        print("Starting RX container in background...")
        return subprocess.Popen(
            [str(self.paths.rx_script)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
    
    def _start_tx_container(self) -> subprocess.Popen:
        """Start TX container in background"""
        print("Starting TX container in background...")
        return subprocess.Popen(
            [str(self.paths.tx_script)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
    
    def _stop_containers(self, processes: List[subprocess.Popen]) -> None:
        """Stop all containers forcefully"""
        print("Timeout reached, stopping Docker containers...")
        subprocess.run(
            "docker ps -q --filter ancestor=holohub:connext_app_cpp | xargs -r docker stop",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        # Terminate processes
        for p in processes:
            try:
                p.terminate()
            except:
                pass
        
        time.sleep(1)
        
        # Force kill if needed
        for p in processes:
            try:
                p.kill()
            except:
                pass
    
    def _collect_outputs(self, tx_process: subprocess.Popen, rx_process: subprocess.Popen) -> Tuple[str, str]:
        """Collect and print outputs from both containers"""
        try:
            tx_output, _ = tx_process.communicate(timeout=2)
        except:
            tx_output = ""
        
        try:
            rx_output, _ = rx_process.communicate(timeout=2)
        except:
            rx_output = ""
        
        print("RX container output:")
        print("-" * 80)
        print(rx_output)
        print("-" * 80)
        
        print("\nTX container output:")
        print("-" * 80)
        print(tx_output)
        print("-" * 80)
        
        return tx_output, rx_output


# =============================================================================
# Unified Test Runner
# =============================================================================

class E2ETestRunner:
    """Unified end-to-end test runner for any transport"""
    
    def __init__(
        self,
        paths: TestScriptPaths,
        validator: EnvironmentValidator,
        parser: OutputParser,
        params: Optional[TestParameters] = None
    ):
        self.paths = paths
        self.validator = validator
        self.parser = parser
        self.params = params or TestParameters()
        self.orchestrator = ContainerOrchestrator(paths, self.params)
    
    def validate_environment(self) -> bool:
        """Validate test environment"""
        return self.validator.validate()
    
    def run_test(self) -> bool:
        """Execute the end-to-end test"""
        # Run containers
        tx_output, rx_output = self.orchestrator.run_containers()
        
        # Parse results
        print_header("Parsing Results")
        results = self.parser.parse(tx_output, rx_output)
        
        # Validate
        return self._validate_results(results)
    
    def _validate_results(self, results: TestResults) -> bool:
        """Validate test results against threshold"""
        print_header("Validation")
        
        if results.tx_sent == 0:
            print_error("TX sent 0 messages - test failed")
            return False
        
        min_required = int(results.tx_sent * self.params.success_threshold)
        
        print(f"Success rate: {results.success_rate:.1f}% ({results.rx_received}/{results.tx_sent})")
        print(f"Minimum required: {min_required} messages ({self.params.success_threshold * 100}%)")
        
        if results.meets_threshold(self.params.success_threshold):
            print_success(f"TEST PASSED - Received {results.rx_received}/{min_required} minimum required")
            return True
        else:
            print_error(f"TEST FAILED - Received only {results.rx_received}/{min_required} minimum required")
            return False
