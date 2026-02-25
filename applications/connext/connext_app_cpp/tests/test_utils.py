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
# Hugepage Utilities
# =============================================================================

def get_free_hugepages() -> Optional[int]:
    """Get the number of free 1GB hugepages available
    
    Returns:
        Number of free hugepages, or None if unable to determine
    """
    try:
        with open('/proc/meminfo', 'r') as f:
            meminfo = f.read()
        
        match = re.search(r'HugePages_Free:\s+(\d+)', meminfo)
        if not match:
            return None
        
        return int(match.group(1))
    except Exception:
        return None


def check_hugepage_availability(required: int, context: str = "operation") -> bool:
    """Check if sufficient hugepages are available
    
    Each DPDK container requires 1 hugepage of 1GB.
    
    Args:
        required: Number of hugepages required
        context: Description of what needs hugepages (for error messages)
        
    Returns:
        True if sufficient hugepages available, False otherwise
    """
    free_hugepages = get_free_hugepages()
    
    if free_hugepages is None:
        print_error("Could not determine available hugepages")
        return False
    
    if free_hugepages < required:
        print_error(f"Insufficient hugepages for {context}: {free_hugepages} available, {required} required")
        print_error(f"Each ANO container requires 1 hugepage (1GB)")
        print_error(f"")
        print_error(f"To allocate more hugepages, run as root:")
        print_error(f"  echo {required} > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages")
        return False
    
    return True


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
    
    def validate_hugepages(self, required_containers: int) -> bool:
        """Validate sufficient hugepages are available
        
        Args:
            required_containers: Number of containers that will run simultaneously
            
        Returns:
            True if sufficient hugepages available, False otherwise
        """
        if not check_hugepage_availability(required_containers, f"{required_containers} containers"):
            return False
        
        free_hugepages = get_free_hugepages()
        print_success(f"Hugepages: {free_hugepages} available, {required_containers} required")
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


# =============================================================================
# Multi-Topology Support (1-to-Many, Many-to-Many)
# =============================================================================

@dataclass
class TopologyTestParameters:
    """Test parameters for multi-topology tests"""
    num_publishers: int
    num_subscribers: int
    port_mapping: dict  # Maps subscriber_id -> list of publisher ports
    timeout_seconds: int = 90
    success_threshold: float = 0.80
    rx_init_wait: int = 10  # Increased: wait for all RX to fully initialize
    rx_stagger_seconds: int = 5  # Increased for GPU memory allocation
    tx_stagger_seconds: int = 5  # Increased for GPU DMA mapping
    
    @classmethod
    def for_1_to_many(cls, num_subscribers: int = 3) -> 'TopologyTestParameters':
        """Factory for 1-to-many topology"""
        # Single TX on port 5000, subscribers on 5001, 5002, 5003, etc.
        port_mapping = {
            sub_id: [5000] for sub_id in range(1, num_subscribers + 1)
        }
        return cls(
            num_publishers=1,
            num_subscribers=num_subscribers,
            port_mapping=port_mapping
        )
    
    @classmethod
    def for_many_to_many(cls, num_publishers: int = 2, num_subscribers: int = 1) -> 'TopologyTestParameters':
        """Factory for many-to-one / many-to-many topology

        Default topology (many-to-one intermediate):
        - Publisher 1 (port 6001) -> Subscriber 1
        - Publisher 2 (port 6002) -> Subscriber 1

        Full many-to-many (num_subscribers=3):
        - Publisher 1 (port 6001) -> Subscribers 1, 2
        - Publisher 2 (port 6002) -> Subscribers 1, 3
        """
        if num_publishers == 2 and num_subscribers == 1:
            # Many-to-one intermediate: both publishers send to the single subscriber
            port_mapping = {
                1: [6001, 6002],  # Sub 1 receives from both Pub 1 and Pub 2
            }
        elif num_publishers == 2 and num_subscribers == 3:
            port_mapping = {
                1: [6001, 6002],  # Sub 1 from Pub 1 and Pub 2
                2: [6001],        # Sub 2 from Pub 1
                3: [6002],        # Sub 3 from Pub 2
            }
        else:
            # Generic mapping - each subscriber receives from all publishers
            port_mapping = {
                sub_id: [6000 + pub_id for pub_id in range(1, num_publishers + 1)]
                for sub_id in range(1, num_subscribers + 1)
            }
        
        return cls(
            num_publishers=num_publishers,
            num_subscribers=num_subscribers,
            port_mapping=port_mapping
        )


@dataclass
class MultiTopologyResults:
    """Results from multi-topology test execution"""
    tx_results: List[TestResults]  # One per publisher
    rx_results: List[TestResults]  # One per subscriber
    topology: TopologyTestParameters
    
    @property
    def total_sent(self) -> int:
        """Total messages sent by all publishers"""
        return sum(r.tx_sent for r in self.tx_results)
    
    @property
    def total_received(self) -> int:
        """Total messages received by all subscribers"""
        return sum(r.rx_received for r in self.rx_results)
    
    @property
    def per_subscriber_received(self) -> dict:
        """Map subscriber_id -> messages received"""
        return {i+1: r.rx_received for i, r in enumerate(self.rx_results)}
    
    @property
    def per_publisher_sent(self) -> dict:
        """Map publisher_id -> messages sent"""
        return {i+1: r.tx_sent for i, r in enumerate(self.tx_results)}
    
    def overall_success_rate(self) -> float:
        """Calculate overall success rate across all participants"""
        if self.total_sent == 0:
            return 0.0
        # For 1-to-many: expected_total = tx_sent * num_subscribers
        # For many-to-many: more complex calculation based on topology
        expected_total = sum(
            self.tx_results[0].tx_sent  # Assume all publishers send same amount
            * len(self.topology.port_mapping[sub_id])
            for sub_id in range(1, self.topology.num_subscribers + 1)
        )
        if expected_total == 0:
            return 0.0
        return (self.total_received / expected_total) * 100.0
    
    def meets_threshold(self, threshold: float) -> bool:
        """Check if all subscribers meet the threshold"""
        for sub_id, rx_result in enumerate(self.rx_results, start=1):
            # Calculate expected messages for this subscriber
            num_publishers_for_sub = len(self.topology.port_mapping[sub_id])
            if num_publishers_for_sub == 0:
                continue
            
            # Assume each publisher sends same amount (use first publisher's count)
            expected = self.tx_results[0].tx_sent * num_publishers_for_sub
            if expected == 0:
                continue
            
            min_required = int(expected * threshold)
            if rx_result.rx_received < min_required:
                return False
        
        return True


class MultiProcessOrchestrator:
    """Orchestrates multiple TX and RX containers for topology tests"""
    
    def __init__(self, topology: TopologyTestParameters, test_type: str = "1_to_many"):
        """
        Args:
            topology: Topology configuration
            test_type: Type of test ("1_to_many" or "many_to_many")
        """
        self.topology = topology
        self.test_type = test_type
        self.script_dir = Path(__file__).parent
    
    def run_containers(self) -> Tuple[List[str], List[str]]:
        """Run TX and RX containers, return (tx_outputs, rx_outputs)"""
        print_header(f"Starting Multi-Topology Test (timeout={self.topology.timeout_seconds}s)")
        print(f"Publishers: {self.topology.num_publishers}, Subscribers: {self.topology.num_subscribers}")

        # Kill any stale containers from previous runs before starting
        self._cleanup_stale_containers()

        # Start all RX containers first (staggered)
        rx_processes = self._start_rx_containers()
        
        # Wait for RX initialization
        print(f"\nWaiting {self.topology.rx_init_wait} seconds for all RX containers to initialize...")
        time.sleep(self.topology.rx_init_wait)
        
        # Start all TX containers (staggered)
        tx_processes = self._start_tx_containers()
        
        # Run test duration
        print(f"\nRunning test for {self.topology.timeout_seconds} seconds...")
        print("(Containers will be terminated after timeout)\n")
        time.sleep(self.topology.timeout_seconds)
        
        # Stop all containers first
        print("\nStopping all Docker containers...")
        subprocess.run(
            "docker ps -q --filter ancestor=holohub:connext_app_cpp | xargs -r docker stop",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        # Give containers a moment to flush output
        time.sleep(2)
        
        # Now collect outputs from the terminated processes
        print("\nCollecting outputs...")
        tx_outputs = self._collect_outputs(tx_processes, "TX")
        rx_outputs = self._collect_outputs(rx_processes, "RX")
        
        return tx_outputs, rx_outputs
    
    def _start_rx_containers(self) -> List[subprocess.Popen]:
        """Start all RX containers with staggering"""
        processes = []
        for sub_id in range(1, self.topology.num_subscribers + 1):
            # Validate hugepage availability before launching
            if not check_hugepage_availability(1, f"RX Subscriber {sub_id}"):
                print_error(f"Failed to start RX Subscriber {sub_id} - insufficient hugepages")
                # Stop already running containers
                self._stop_all_containers(processes)
                raise RuntimeError(f"Insufficient hugepages for RX Subscriber {sub_id}")
            
            script = self.script_dir / "scripts" / f"run_e2e_rx_{self.test_type}_container.sh"
            print(f"Starting RX Subscriber {sub_id}...")
            print(f"  Script: {script}")
            print(f"  Exists: {script.exists()}")
            
            process = subprocess.Popen(
                [str(script), str(sub_id)],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True
            )
            processes.append(process)
            
            # Wait for Docker to reserve hugepages before next container
            # This prevents race condition where validation passes but Docker hasn't
            # actually reserved the hugepages yet
            time.sleep(2)
            
            # Additional stagger for RX startups
            if sub_id < self.topology.num_subscribers:
                time.sleep(self.topology.rx_stagger_seconds)
        
        return processes
    
    def _start_tx_containers(self) -> List[subprocess.Popen]:
        """Start all TX containers with staggering"""
        processes = []
        
        if self.topology.num_publishers == 1:
            # Validate hugepage availability before launching
            if not check_hugepage_availability(1, "TX Publisher"):
                print_error("Failed to start TX Publisher - insufficient hugepages")
                raise RuntimeError("Insufficient hugepages for TX Publisher")
            
            # Single TX for 1-to-many
            script = self.script_dir / "scripts" / f"run_e2e_tx_{self.test_type}_container.sh"
            print(f"Starting TX Publisher...")
            print(f"  Script: {script}")
            print(f"  Exists: {script.exists()}")
            
            process = subprocess.Popen(
                [str(script)],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True
            )
            processes.append(process)
            
            # Wait for Docker to reserve hugepages
            time.sleep(2)
        else:
            # Multiple TX for many-to-many
            for pub_id in range(1, self.topology.num_publishers + 1):
                # Validate hugepage availability before launching
                if not check_hugepage_availability(1, f"TX Publisher {pub_id}"):
                    print_error(f"Failed to start TX Publisher {pub_id} - insufficient hugepages")
                    # Stop already running containers
                    self._stop_all_containers(processes)
                    raise RuntimeError(f"Insufficient hugepages for TX Publisher {pub_id}")
                
                script = self.script_dir / "scripts" / f"run_e2e_tx_{self.test_type}_container.sh"
                print(f"Starting TX Publisher {pub_id}...")
                
                process = subprocess.Popen(
                    [str(script), str(pub_id)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True
                )
                processes.append(process)
                
                # Wait for Docker to reserve hugepages before next container
                time.sleep(2)
                
                # Additional stagger for TX startups
                if pub_id < self.topology.num_publishers:
                    time.sleep(self.topology.tx_stagger_seconds)
        
        return processes
    
    def _cleanup_stale_containers(self) -> None:
        """Kill any connext_app_cpp containers left over from previous runs"""
        result = subprocess.run(
            "docker ps -q --filter ancestor=holohub:connext_app_cpp",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stale_ids = result.stdout.strip()
        if stale_ids:
            count = len(stale_ids.splitlines())
            print(f"WARNING: Found {count} stale container(s) from a previous run — stopping them now...")
            subprocess.run(
                f"docker stop {stale_ids.replace(chr(10), ' ')}",
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            time.sleep(2)  # Allow hugepages to be released
            print("Stale containers stopped.")
        else:
            print("No stale containers found.")

    def _stop_all_containers(self, processes: List[subprocess.Popen]) -> None:
        """Stop all containers forcefully"""
        print("\nStopping all Docker containers...")
        subprocess.run(
            "docker ps -q --filter ancestor=holohub:connext_app_cpp | xargs -r docker stop",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        # Terminate all processes
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
    
    def _collect_outputs_non_blocking(self, processes: List[subprocess.Popen], label: str) -> List[str]:
        """Collect outputs from running processes without terminating them
        
        This reads whatever output is available in stdout without blocking.
        """
        outputs = []
        for i, process in enumerate(processes, start=1):
            try:
                # Read available output without blocking
                # The process is still running, so we just read what's buffered
                import select
                output_lines = []
                
                # Set stdout to non-blocking mode and read available data
                if process.stdout:
                    import fcntl
                    import os
                    flags = fcntl.fcntl(process.stdout, fcntl.F_GETFL)
                    fcntl.fcntl(process.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)
                    
                    try:
                        while True:
                            line = process.stdout.readline()
                            if not line:
                                break
                            output_lines.append(line)
                    except:
                        pass  # No more data available
                    
                    # Restore blocking mode
                    fcntl.fcntl(process.stdout, fcntl.F_SETFL, flags)
                
                output = ''.join(output_lines)
            except Exception as e:
                output = f"ERROR collecting output from {label} {i}: {str(e)}"
            
            print(f"\n{label} Container {i} output:")
            print("-" * 80)
            if output:
                # Print first 100 lines only to avoid flooding console
                lines = output.split('\n')
                if len(lines) > 100:
                    print('\n'.join(lines[:50]))
                    print(f"\n... ({len(lines) - 100} lines omitted) ...\n")
                    print('\n'.join(lines[-50:]))
                else:
                    print(output)
            else:
                print(f"(No output captured from {label} {i})")
            print("-" * 80)
            
            outputs.append(output if output else "")
        
        return outputs
    
    def _collect_outputs(self, processes: List[subprocess.Popen], label: str) -> List[str]:
        """Collect outputs from multiple processes"""
        outputs = []
        for i, process in enumerate(processes, start=1):
            try:
                output, _ = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                print(f"WARNING: {label} Container {i} did not terminate within timeout, forcing kill...")
                process.kill()
                try:
                    output, _ = process.communicate(timeout=2)
                except:
                    output = f"ERROR: Could not get output from {label} {i}"
            except Exception as e:
                output = f"ERROR collecting output from {label} {i}: {str(e)}"
            
            print(f"\n{label} Container {i} output:")
            print("-" * 80)
            print(output if output else f"(No output from {label} {i})")
            print("-" * 80)
            
            outputs.append(output if output else "")
        
        return outputs


class MultiTopologyOutputParser:
    """Parser for multi-topology test outputs"""
    
    def __init__(self, base_parser: OutputParser):
        self.base_parser = base_parser
    
    def parse_multiple(
        self,
        tx_outputs: List[str],
        rx_outputs: List[str],
        topology: TopologyTestParameters
    ) -> MultiTopologyResults:
        """Parse multiple TX and RX outputs.

        For many-to-many/many-to-one tests the payload prefix is per-publisher
        (pub1_payload, pub2_payload, …), so we count occurrences of
        'PayloadSource sending payload: pubN_payload' directly rather than
        relying on the base parser's fixed pattern.
        """
        print_header("Parsing Multi-Topology Results")

        # Parse each TX output by counting its own publisher prefix
        tx_results = []
        for i, tx_output in enumerate(tx_outputs, start=1):
            prefix = f"pub{i}_payload"
            sent = tx_output.count(f"PayloadSource sending payload: {prefix}")
            if sent == 0:
                # Fallback: any PayloadSource line (covers non-prefixed configs)
                sent = len(re.findall(r'PayloadSource sending payload:', tx_output))
            print(f"\nParsing TX Publisher {i}:")
            if sent:
                print_success(f"  Sent: {sent} messages (prefix '{prefix}')")
            else:
                print_error(f"  Could not find sent messages for prefix '{prefix}'")
            tx_results.append(TestResults(tx_sent=sent, rx_received=0,
                                          tx_output=tx_output, rx_output=""))

        # Parse each RX output by counting all recognised publisher prefixes
        rx_results = []
        for i, rx_output in enumerate(rx_outputs, start=1):
            # Sum occurrences of every publisher prefix that appears in RX output
            total_received = 0
            for pub_id in range(1, topology.num_publishers + 1):
                prefix = f"pub{pub_id}_payload"
                total_received += rx_output.count(prefix)

            if total_received == 0:
                # Fallback: summary line "Receiver collected N payload(s)"
                match = re.search(r'Receiver collected (\d+) payload\(s\)', rx_output)
                if match:
                    total_received = int(match.group(1))

            print(f"\nParsing RX Subscriber {i}:")
            if total_received:
                print_success(f"  Received: {total_received} messages (all publishers)")
            else:
                print_error(f"  Could not verify reception for Subscriber {i}")
            rx_results.append(TestResults(tx_sent=0, rx_received=total_received,
                                          tx_output="", rx_output=rx_output))

        return MultiTopologyResults(tx_results, rx_results, topology)
    
    def validate_subscriber_isolation(self, results: MultiTopologyResults) -> bool:
        """Validate no message duplication across subscribers"""
        # For 1-to-many, total received should not exceed num_subscribers * tx_sent
        if results.topology.num_publishers == 1:
            max_expected = results.tx_results[0].tx_sent * results.topology.num_subscribers
            if results.total_received > max_expected:
                print_error(f"Message duplication detected: received {results.total_received}, max expected {max_expected}")
                return False
            print_success("No message duplication detected")
        return True
