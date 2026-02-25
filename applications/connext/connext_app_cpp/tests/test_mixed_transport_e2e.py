#!/usr/bin/env python3
"""
Mixed-Transport End-to-End Test (ANO + DDS)

Tests one dual-mode publisher sending simultaneously over both transports
to two subscribers: RX1 receives via ANO (DPDK), RX2 receives via DDS.

Topology:
    TX (ANO + DDS)
      ├─── [ANO / DPDK] ──► RX1 (ANO)   : validates ANO packet delivery
      └─── [DDS / RTI]  ──► RX2 (DDS)   : validates DDS delivery
    
SOLID Design:
    - MixedTransportOrchestrator handles asymmetric RX launch (DDS first, no
      hugepages; ANO second, needs hugepages) without modifying MultiProcessOrchestrator.
    - MixedOutputParser subclasses ANOOutputParser / DDSOutputParser using the
      correct payload prefix 'mixed_test_payload' instead of the default patterns.
    - Transport isolation check ensures no cross-contamination in logs.

Rate-based testing with timeout control:
    - TX sends at 10 msg/sec for 90 seconds (~850 messages per transport)
    - RX1 and RX2 each run for 90 seconds
    - Test validates that BOTH RX paths received at least 80% of TX messages
    - Additionally verifies no ANO log lines appear in RX2 and no DDS log lines in RX1

Hugepage budget:
    - TX  : 1 hugepage (ANO/DPDK)
    - RX1 : 1 hugepage (ANO/DPDK)
    - RX2 : 0 hugepages (DDS only)
    Total: 2 hugepages required

Prerequisites:
    - Physical NICs: TEST_TX_NIC_PCIE and TEST_RX_NIC_PCIE environment variables
    - RTI license: RTI_LICENSE_FILE or ./rti_license.dat
    - NICs connected via cable or network switch
    - At least 4 free 1 GB hugepages recommended (2 required)

Usage:
    export TEST_TX_NIC_PCIE="0005:03:00.0"
    export TEST_RX_NIC_PCIE="0005:03:00.1"
    export RTI_LICENSE_FILE="./rti_license.dat"
    python3 test_mixed_transport_e2e.py

    # Custom timeout and threshold
    python3 test_mixed_transport_e2e.py --timeout 120 --threshold 0.75

Exit codes:
    0 - PASS: both paths met threshold, transport isolation confirmed
    1 - Environment / setup failure
    2 - ANO path (RX1) below threshold
    3 - DDS path (RX2) below threshold
    4 - Transport isolation violation detected
"""

import os
import re
import sys
import argparse
import subprocess
import time
from pathlib import Path

from test_utils import (
    ANOEnvironmentValidator,
    ANOOutputParser,
    DDSOutputParser,
    OutputParser,
    TestResults,
    TestScriptPaths,
    check_hugepage_availability,
    get_free_hugepages,
    print_header,
    print_success,
    print_error,
)


# =============================================================================
# Custom parsers for "mixed_test_payload" prefix
# =============================================================================

class MixedANOOutputParser(ANOOutputParser):
    """ANO parser that matches 'mixed_test_payload_#N' instead of default"""

    def get_payload_pattern(self) -> str:
        return r'mixed_test_payload_#(\d+)'


class MixedDDSOutputParser(DDSOutputParser):
    """DDS parser that matches 'mixed_test_payload_#N' instead of default"""

    def get_payload_pattern(self) -> str:
        return r'mixed_test_payload_#(\d+)'


# =============================================================================
# Argument Parsing
# =============================================================================

def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments"""
    parser = argparse.ArgumentParser(
        description="Mixed-Transport End-to-End Test (ANO + DDS)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument(
        '--timeout',
        type=int,
        default=90,
        help='Test duration in seconds (default: 90)'
    )

    parser.add_argument(
        '--threshold',
        type=float,
        default=0.80,
        help='Success threshold as fraction (default: 0.80)'
    )

    parser.add_argument(
        '--rx-init-wait',
        type=int,
        default=5,
        help='Seconds to wait after starting RX containers before starting TX (default: 5)'
    )

    return parser.parse_args()


# =============================================================================
# Environment Validation
# =============================================================================

def validate_environment(script_dir: Path, required_hugepages: int) -> bool:
    """Validate environment for mixed-transport testing.

    Args:
        script_dir: Directory containing the test scripts
        required_hugepages: Number of 1 GB hugepages required (TX + RX1-ANO)

    Returns:
        True if environment is valid, False otherwise
    """
    print_header("Validating Environment")

    # RTI license
    rti_license = os.getenv("RTI_LICENSE_FILE", "./rti_license.dat")
    if not os.path.exists(rti_license):
        print_error(f"RTI license file not found: {rti_license}")
        return False
    print_success(f"RTI License: {rti_license}")

    # NIC PCIe addresses (needed for ANO containers: TX + RX1)
    tx_nic = os.getenv("TEST_TX_NIC_PCIE")
    rx_nic = os.getenv("TEST_RX_NIC_PCIE")

    if not tx_nic:
        print_error("TEST_TX_NIC_PCIE environment variable not set (required for ANO TX)")
        return False
    if not rx_nic:
        print_error("TEST_RX_NIC_PCIE environment variable not set (required for ANO RX1)")
        return False

    print_success(f"TX NIC PCIe: {tx_nic}")
    print_success(f"RX NIC PCIe: {rx_nic}")

    # Required scripts and configs
    required_files = [
        script_dir / "scripts" / "run_e2e_tx_mixed_container.sh",
        script_dir / "scripts" / "run_e2e_rx_mixed_ano_container.sh",
        script_dir / "scripts" / "run_e2e_rx_mixed_dds_container.sh",
        script_dir / "config"  / "test_mixed_tx.yaml",
        script_dir / "config"  / "test_mixed_rx_ano.yaml",
        script_dir / "config"  / "test_mixed_rx_dds.yaml",
    ]

    for f in required_files:
        if not f.exists():
            print_error(f"Required file not found: {f}")
            return False
    print_success("All required scripts and config files found")

    # Hugepages (only TX and RX1 need them)
    if not check_hugepage_availability(required_hugepages,
                                       f"mixed-transport test ({required_hugepages} ANO containers)"):
        return False

    free = get_free_hugepages()
    print_success(f"Hugepages: {free} available, {required_hugepages} required")

    return True


# =============================================================================
# Mixed-Transport Orchestrator
# =============================================================================

class MixedTransportOrchestrator:
    """Launches and manages the 3 containers for the mixed-transport test.

    Startup order is deliberate:
      1. RX2 (DDS)  — no DPDK/hugepages; start first so DDS discovery is ready
      2. wait 2 s
      3. RX1 (ANO)  — needs hugepages; DPDK init ~3-4 s
      4. wait rx_init_wait s total
      5. TX (ANO+DDS dual-mode)

    This mirrors the ordering used in other orchestrators and avoids the hugepage
    race condition seen when starting multiple DPDK containers simultaneously.
    """

    def __init__(self, script_dir: Path, timeout_seconds: int, rx_init_wait: int):
        self.script_dir = script_dir
        self.timeout_seconds = timeout_seconds
        self.rx_init_wait = rx_init_wait

        self._tx_script  = script_dir / "scripts" / "run_e2e_tx_mixed_container.sh"
        self._rx1_script = script_dir / "scripts" / "run_e2e_rx_mixed_ano_container.sh"
        self._rx2_script = script_dir / "scripts" / "run_e2e_rx_mixed_dds_container.sh"

    def run(self):
        """Run all containers and return (tx_output, rx1_output, rx2_output)."""
        print_header(f"Starting Mixed-Transport Test (timeout={self.timeout_seconds}s)")

        # ── cleanup stale containers ──────────────────────────────────────────
        self._cleanup_stale_containers()

        # ── start RX2 (DDS) first — no hugepages required ────────────────────
        print("Starting RX2 (DDS subscriber)...")
        rx2_proc = subprocess.Popen(
            [str(self._rx2_script)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        print("  RX2 (DDS) started — waiting 2 s for DDS discovery pre-warm...")
        time.sleep(2)

        # ── start RX1 (ANO) — needs 1 hugepage ───────────────────────────────
        if not check_hugepage_availability(1, "RX1 (ANO subscriber)"):
            print_error("Insufficient hugepages for RX1; aborting")
            self._stop_all([rx2_proc])
            raise RuntimeError("Insufficient hugepages for RX1 (ANO)")

        print("Starting RX1 (ANO subscriber)...")
        rx1_proc = subprocess.Popen(
            [str(self._rx1_script)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        # Allow Docker to reserve hugepages before next DPDK container
        time.sleep(2)

        # ── wait for all RX containers to be ready ────────────────────────────
        remaining_wait = max(0, self.rx_init_wait - 2)
        if remaining_wait > 0:
            print(f"Waiting {remaining_wait} more seconds for RX containers to initialize...")
            time.sleep(remaining_wait)

        # ── start TX (dual-mode: ANO + DDS) — needs 1 hugepage ───────────────
        if not check_hugepage_availability(1, "TX (dual-mode ANO+DDS)"):
            print_error("Insufficient hugepages for TX; aborting")
            self._stop_all([rx1_proc, rx2_proc])
            raise RuntimeError("Insufficient hugepages for TX (ANO+DDS)")

        print("Starting TX (dual-mode ANO+DDS publisher)...")
        tx_proc = subprocess.Popen(
            [str(self._tx_script)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )
        time.sleep(2)  # Allow Docker to reserve TX hugepage

        # ── run test for the configured duration ──────────────────────────────
        print(f"\nRunning test for {self.timeout_seconds} seconds...")
        print("(All containers will be stopped after timeout)\n")
        time.sleep(self.timeout_seconds)

        # ── stop all containers ───────────────────────────────────────────────
        print("\nStopping all Docker containers...")
        subprocess.run(
            "docker ps -q --filter ancestor=holohub:connext_app_cpp | xargs -r docker stop",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(2)  # Let containers flush output

        # ── collect outputs ───────────────────────────────────────────────────
        tx_output  = self._collect(tx_proc,  "TX (dual-mode)")
        rx1_output = self._collect(rx1_proc, "RX1 (ANO)")
        rx2_output = self._collect(rx2_proc, "RX2 (DDS)")

        return tx_output, rx1_output, rx2_output

    # ── helpers ───────────────────────────────────────────────────────────────

    def _collect(self, proc: subprocess.Popen, label: str) -> str:
        """Collect output from a single process."""
        try:
            output, _ = proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            print(f"WARNING: {label} container did not terminate within timeout — force killing...")
            proc.kill()
            try:
                output, _ = proc.communicate(timeout=2)
            except Exception:
                output = f"ERROR: Could not get output from {label}"
        except Exception as e:
            output = f"ERROR collecting output from {label}: {e}"

        print(f"\n{label} container output:")
        print("-" * 80)
        lines = (output or "").split('\n')
        if len(lines) > 100:
            print('\n'.join(lines[:50]))
            print(f"\n... ({len(lines) - 100} lines omitted) ...\n")
            print('\n'.join(lines[-50:]))
        else:
            print(output or f"(No output from {label})")
        print("-" * 80)

        return output or ""

    def _stop_all(self, procs) -> None:
        """Stop Docker containers and terminate processes."""
        subprocess.run(
            "docker ps -q --filter ancestor=holohub:connext_app_cpp | xargs -r docker stop",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        for p in procs:
            try:
                p.terminate()
            except Exception:
                pass
        time.sleep(1)
        for p in procs:
            try:
                p.kill()
            except Exception:
                pass

    def _cleanup_stale_containers(self) -> None:
        """Kill any connext_app_cpp containers left from previous runs."""
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
            print(f"WARNING: Found {count} stale container(s) — stopping them now...")
            subprocess.run(
                f"docker stop {stale_ids.replace(chr(10), ' ')}",
                shell=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            time.sleep(2)
            print("Stale containers stopped.")
        else:
            print("No stale containers found.")


# =============================================================================
# Transport Isolation Checker
# =============================================================================

# Keywords that indicate DDS activity in a log stream
_DDS_INDICATORS = [
    'DDS DataWriter',
    'DDS DataReader',
    'dds_topic',
    'domain_id',
]

# Keywords that indicate ANO/DPDK activity in a log stream
_ANO_INDICATORS = [
    'DPDK',
    'advanced_network',
    'ANO',
    'PCI:',
    'Port 0:',
]


def check_transport_isolation(rx1_output: str, rx2_output: str) -> bool:
    """Verify transport logs are not cross-contaminated.

    Args:
        rx1_output: Log output from the ANO subscriber (RX1)
        rx2_output: Log output from the DDS subscriber (RX2)

    Returns:
        True if no cross-contamination detected, False otherwise
    """
    print_header("Checking Transport Isolation")

    passed = True

    # RX1 (ANO) should NOT contain DDS indicators
    dds_in_rx1 = [kw for kw in _DDS_INDICATORS if kw in rx1_output]
    if dds_in_rx1:
        print_error(
            f"RX1 (ANO) log contains DDS indicators: {dds_in_rx1}\n"
            "  → DDS traffic may be leaking into the ANO subscriber"
        )
        passed = False
    else:
        print_success("RX1 (ANO) log contains no DDS indicators ✓")

    # RX2 (DDS) should NOT contain ANO/DPDK indicators
    ano_in_rx2 = [kw for kw in _ANO_INDICATORS if kw in rx2_output]
    if ano_in_rx2:
        print_error(
            f"RX2 (DDS) log contains ANO/DPDK indicators: {ano_in_rx2}\n"
            "  → DPDK traffic may be leaking into the DDS subscriber"
        )
        passed = False
    else:
        print_success("RX2 (DDS) log contains no ANO/DPDK indicators ✓")

    return passed


# =============================================================================
# Main
# =============================================================================

def main():
    """Mixed-transport test entry point."""
    args = parse_arguments()

    script_dir = Path(__file__).parent

    # ── hugepage budget ───────────────────────────────────────────────────────
    # TX(1 ANO) + RX1(1 ANO) + RX2(0 DDS) = 2 containers needing hugepages
    REQUIRED_HUGEPAGES = 2

    # ── validate environment ──────────────────────────────────────────────────
    if not validate_environment(script_dir, REQUIRED_HUGEPAGES):
        print_error("Environment validation failed")
        sys.exit(1)

    # ── run containers ────────────────────────────────────────────────────────
    orchestrator = MixedTransportOrchestrator(
        script_dir=script_dir,
        timeout_seconds=args.timeout,
        rx_init_wait=args.rx_init_wait
    )

    try:
        tx_output, rx1_output, rx2_output = orchestrator.run()
    except RuntimeError as e:
        print_error(f"Container launch failed: {e}")
        print_error("Test aborted due to insufficient resources")
        sys.exit(1)

    # ── parse TX output (dual-mode TX produces both patterns; count either) ───
    print_header("Parsing TX Output")
    # TX uses payload prefix 'mixed_test_payload' for both paths
    tx_ano_sent = len(re.findall(r'PayloadSource sending payload: mixed_test_payload_#\d+', tx_output))
    if tx_ano_sent == 0:
        # Fallback: any PayloadSource line
        tx_ano_sent = len(re.findall(r'PayloadSource sending payload:', tx_output))

    if tx_ano_sent:
        print_success(f"TX sent: {tx_ano_sent} messages")
    else:
        print_error("TX sent 0 messages — test failed")
        sys.exit(1)

    # ── parse RX1 (ANO) ───────────────────────────────────────────────────────
    print_header("Parsing RX1 (ANO) Output")
    ano_parser = MixedANOOutputParser()
    rx1_result: TestResults = ano_parser.parse(tx_output, rx1_output)
    # Override tx_sent with actual count (parse() uses its own pattern on tx_output)
    rx1_result = TestResults(
        tx_sent=tx_ano_sent,
        rx_received=rx1_result.rx_received,
        tx_output=tx_output,
        rx_output=rx1_output
    )

    # ── parse RX2 (DDS) ───────────────────────────────────────────────────────
    print_header("Parsing RX2 (DDS) Output")
    dds_parser = MixedDDSOutputParser()
    rx2_result: TestResults = dds_parser.parse(tx_output, rx2_output)
    rx2_result = TestResults(
        tx_sent=tx_ano_sent,
        rx_received=rx2_result.rx_received,
        tx_output=tx_output,
        rx_output=rx2_output
    )

    # ── transport isolation check ─────────────────────────────────────────────
    isolation_ok = check_transport_isolation(rx1_output, rx2_output)

    # ── validation ────────────────────────────────────────────────────────────
    print_header("Validation")

    threshold = args.threshold
    min_required = int(tx_ano_sent * threshold)

    # RX1 (ANO)
    rx1_rate = rx1_result.success_rate
    rx1_pass = rx1_result.meets_threshold(threshold)
    print(f"\nRX1 (ANO path):")
    print(f"  Received: {rx1_result.rx_received}/{tx_ano_sent} messages")
    print(f"  Success rate: {rx1_rate:.1f}%")
    print(f"  Minimum required: {min_required} ({threshold * 100:.0f}%)")
    if rx1_pass:
        print_success("  RX1 (ANO) PASSED")
    else:
        print_error("  RX1 (ANO) FAILED — below threshold")

    # RX2 (DDS)
    rx2_rate = rx2_result.success_rate
    rx2_pass = rx2_result.meets_threshold(threshold)
    print(f"\nRX2 (DDS path):")
    print(f"  Received: {rx2_result.rx_received}/{tx_ano_sent} messages")
    print(f"  Success rate: {rx2_rate:.1f}%")
    print(f"  Minimum required: {min_required} ({threshold * 100:.0f}%)")
    if rx2_pass:
        print_success("  RX2 (DDS) PASSED")
    else:
        print_error("  RX2 (DDS) FAILED — below threshold")

    # ── summary ───────────────────────────────────────────────────────────────
    print("\n" + "=" * 80)
    print("Overall Results:")
    print(f"  TX sent:             {tx_ano_sent} messages")
    print(f"  RX1 (ANO) received:  {rx1_result.rx_received} messages  ({rx1_rate:.1f}%)")
    print(f"  RX2 (DDS) received:  {rx2_result.rx_received} messages  ({rx2_rate:.1f}%)")
    print(f"  Transport isolation: {'✓ OK' if isolation_ok else '✗ VIOLATED'}")
    print("=" * 80)

    # ── exit codes ────────────────────────────────────────────────────────────
    if not rx1_pass:
        print_header("✗ TEST FAILED — ANO path (RX1) below threshold")
        sys.exit(2)

    if not rx2_pass:
        print_header("✗ TEST FAILED — DDS path (RX2) below threshold")
        sys.exit(3)

    if not isolation_ok:
        print_header("✗ TEST FAILED — Transport isolation violation detected")
        sys.exit(4)

    print_header("✓ TEST PASSED — Both paths met threshold, transport isolation confirmed")
    sys.exit(0)


if __name__ == "__main__":
    main()
