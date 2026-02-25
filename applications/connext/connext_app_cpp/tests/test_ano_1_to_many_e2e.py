#!/usr/bin/env python3
"""
ANO 1-to-Many End-to-End Test

Tests one publisher sending to N subscribers (N=3 by default).
Each subscriber listens on a different UDP port.

Rate-based testing with timeout control:
- TX sends at 10 msg/sec for 90 seconds (~850 messages)
- Each RX receives for 90 seconds
- Test validates that each RX received at least 80% of TX messages

Prerequisites:
    - Physical NICs with TEST_TX_NIC_PCIE and TEST_RX_NIC_PCIE environment variables
    - RTI license file (RTI_LICENSE_FILE or ./rti_license.dat)
    - NICs connected via cable or network switch
    - Same hardware requirements as test_ano_e2e.py

Usage:
    # Basic usage with defaults (3 subscribers)
    export TEST_TX_NIC_PCIE="0005:03:00.0"
    export TEST_RX_NIC_PCIE="0005:03:00.1"
    export RTI_LICENSE_FILE="./rti_license.dat"
    python3 test_ano_1_to_many_e2e.py
    
    # Custom number of subscribers
    python3 test_ano_1_to_many_e2e.py --num-subscribers 2
    
    # With custom timeout
    python3 test_ano_1_to_many_e2e.py --num-subscribers 4 --timeout 120
"""

import sys
import argparse
from test_utils import (
    ANOEnvironmentValidator,
    ANOOutputParser,
    TopologyTestParameters,
    MultiProcessOrchestrator,
    MultiTopologyOutputParser,
    print_header,
    print_success,
    print_error
)


def parse_arguments():
    """Parse command-line arguments"""
    parser = argparse.ArgumentParser(
        description="ANO 1-to-Many End-to-End Test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        '--num-subscribers',
        type=int,
        default=3,
        help='Number of subscribers (default: 3)'
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
    
    return parser.parse_args()


def validate_environment(num_containers: int):
    """Validate environment for ANO testing
    
    Args:
        num_containers: Total number of containers that will run (publishers + subscribers)
    """
    # Create minimal paths object just for validation
    from pathlib import Path
    from test_utils import TestScriptPaths
    
    script_dir = Path(__file__).parent
    paths = TestScriptPaths(
        rx_script=script_dir / "scripts" / "run_e2e_rx_1_to_many_container.sh",
        tx_script=script_dir / "scripts" / "run_e2e_tx_1_to_many_container.sh",
        rx_config=script_dir / "config" / "test_ano_rx_1_to_many_sub1.yaml",
        tx_config=script_dir / "config" / "test_ano_tx_1_to_many.yaml"
    )
    
    validator = ANOEnvironmentValidator(paths)
    if not validator.validate():
        return False
    
    # Validate hugepages for multi-container test
    if not validator.validate_hugepages(num_containers):
        return False
    
    return True


def validate_topology(num_subscribers: int) -> bool:
    """Validate topology parameters"""
    print_header("Validating Topology Parameters")
    
    if num_subscribers < 1:
        print_error(f"Number of subscribers must be at least 1, got {num_subscribers}")
        return False
    
    if num_subscribers > 10:
        print_error(f"Number of subscribers exceeds maximum of 10, got {num_subscribers}")
        return False
    
    print_success(f"Topology valid: 1 publisher → {num_subscribers} subscribers")
    return True


def main():
    """ANO 1-to-many test entry point"""
    # Parse arguments
    args = parse_arguments()
    
    # Calculate total containers needed
    total_containers = 1 + args.num_subscribers  # 1 TX + N RX
    
    # Validate environment (NICs, license, hugepages)
    if not validate_environment(total_containers):
        print_error("Environment validation failed")
        sys.exit(1)
    
    # Validate topology parameters
    if not validate_topology(args.num_subscribers):
        print_error("Topology validation failed")
        sys.exit(1)
    
    # Create topology configuration
    topology = TopologyTestParameters.for_1_to_many(num_subscribers=args.num_subscribers)
    topology.timeout_seconds = args.timeout
    topology.success_threshold = args.threshold
    
    # Create orchestrator
    orchestrator = MultiProcessOrchestrator(topology, test_type="1_to_many")
    
    # Run containers (with per-container hugepage validation)
    try:
        tx_outputs, rx_outputs = orchestrator.run_containers()
    except RuntimeError as e:
        print_error(f"Container launch failed: {e}")
        print_error("Test aborted due to insufficient resources")
        sys.exit(2)
    
    # Parse results
    parser = MultiTopologyOutputParser(ANOOutputParser())
    results = parser.parse_multiple(tx_outputs, rx_outputs, topology)
    
    # Validate subscriber isolation (no duplication)
    if not parser.validate_subscriber_isolation(results):
        print_header("✗ TEST FAILED - Message Duplication Detected")
        sys.exit(3)
    
    # Validate results
    print_header("Validation")
    
    if results.total_sent == 0:
        print_error("TX sent 0 messages - test failed")
        sys.exit(1)
    
    # Check each subscriber individually
    all_passed = True
    for sub_id in range(1, args.num_subscribers + 1):
        rx_result = results.rx_results[sub_id - 1]
        expected = results.tx_results[0].tx_sent
        min_required = int(expected * args.threshold)
        
        success_rate = (rx_result.rx_received / expected * 100.0) if expected > 0 else 0.0
        
        print(f"\nSubscriber {sub_id}:")
        print(f"  Received: {rx_result.rx_received}/{expected} messages")
        print(f"  Success rate: {success_rate:.1f}%")
        print(f"  Minimum required: {min_required} ({args.threshold * 100}%)")
        
        if rx_result.rx_received >= min_required:
            print_success(f"  Subscriber {sub_id} PASSED")
        else:
            print_error(f"  Subscriber {sub_id} FAILED - Below threshold")
            all_passed = False
    
    # Summary
    print("\n" + "=" * 80)
    print(f"Overall Results:")
    print(f"  TX sent: {results.total_sent} messages")
    print(f"  Total RX received: {results.total_received} messages")
    print(f"  Overall success rate: {results.overall_success_rate():.1f}%")
    print("=" * 80)
    
    if all_passed:
        print_header("✓ TEST PASSED - All subscribers met threshold")
        sys.exit(0)
    else:
        print_header("✗ TEST FAILED - One or more subscribers below threshold")
        sys.exit(2)


if __name__ == "__main__":
    main()
