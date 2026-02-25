#!/usr/bin/env python3
"""
ANO Many-to-One / Many-to-Many End-to-End Test

Intermediate many-to-one topology (default): 2 publishers, 1 subscriber.
The single subscriber receives from both publishers simultaneously,
validating that the ANO RX dual-flow configuration works correctly before
testing a full many-to-many scenario.

Default topology (2 publishers, 1 subscriber):
- Publisher 1 (port 6001) → Subscriber 1
- Publisher 2 (port 6002) → Subscriber 1
- Subscriber 1 receives from BOTH publishers via dual DPDK flow rules

Rate-based testing with timeout control:
- Each TX sends at 10 msg/sec for 90 seconds (~850 messages)
- The RX receives for 90 seconds
- Test validates that the RX received at least 80% from each configured publisher

Prerequisites:
    - Physical NICs with TEST_TX_NIC_PCIE and TEST_RX_NIC_PCIE environment variables
    - RTI license file (RTI_LICENSE_FILE or ./rti_license.dat)
    - NICs connected via cable or network switch
    - Same hardware requirements as test_ano_e2e.py

Usage:
    # Basic usage with defaults (2 publishers, 1 subscriber = many-to-one)
    export TEST_TX_NIC_PCIE="0005:03:00.0"
    export TEST_RX_NIC_PCIE="0005:03:00.1"
    export RTI_LICENSE_FILE="./rti_license.dat"
    python3 test_ano_many_to_many_e2e.py

    # Full many-to-many (requires corresponding config files)
    python3 test_ano_many_to_many_e2e.py --num-publishers 2 --num-subscribers 3

    # With custom timeout and threshold
    python3 test_ano_many_to_many_e2e.py --timeout 120 --threshold 0.75
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
        description="ANO Many-to-Many End-to-End Test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        '--num-publishers',
        type=int,
        default=2,
        help='Number of publishers (default: 2)'
    )
    
    parser.add_argument(
        '--num-subscribers',
        type=int,
        default=1,
        help='Number of subscribers (default: 1, many-to-one intermediate topology)'
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
        rx_script=script_dir / "scripts" / "run_e2e_rx_many_to_many_container.sh",
        tx_script=script_dir / "scripts" / "run_e2e_tx_many_to_many_container.sh",
        rx_config=script_dir / "config" / "test_ano_rx_many_to_many_sub1.yaml",
        tx_config=script_dir / "config" / "test_ano_tx_many_to_many_pub1.yaml"
    )
    
    validator = ANOEnvironmentValidator(paths)
    if not validator.validate():
        return False
    
    # Validate hugepages for multi-container test
    if not validator.validate_hugepages(num_containers):
        return False
    
    return True


def validate_topology(num_publishers: int, num_subscribers: int) -> bool:
    """Validate topology parameters"""
    print_header("Validating Topology Parameters")
    
    if num_publishers < 1:
        print_error(f"Number of publishers must be at least 1, got {num_publishers}")
        return False

    if num_subscribers < 1:
        print_error(f"Number of subscribers must be at least 1, got {num_subscribers}")
        return False

    if num_publishers > 10:
        print_error(f"Number of publishers exceeds maximum of 10, got {num_publishers}")
        return False

    if num_subscribers > 10:
        print_error(f"Number of subscribers exceeds maximum of 10, got {num_subscribers}")
        return False

    # Describe the topology being used
    if num_publishers == 2 and num_subscribers == 1:
        print_success("Using many-to-one intermediate topology (Intermediate A):")
        print("  Publisher 1 (port 6001) → Subscriber 1")
        print("  Publisher 2 (port 6002) → Subscriber 1")
        print("  Subscriber 1 receives from BOTH publishers (dual DPDK flow rules)")
    elif num_publishers == 2 and num_subscribers == 2:
        print_success("Using many-to-two intermediate topology (Intermediate B):")
        print("  Publisher 1 (src 6001) → Subscriber 1 (dst 6001), Subscriber 2 (dst 6002)")
        print("  Publisher 2 (src 6002) → Subscriber 1 (dst 6001), Subscriber 2 (dst 6002)")
        print("  Both subscribers receive from BOTH publishers (dual DPDK flow rules each)")
    elif num_publishers == 2 and num_subscribers == 3:
        print_success("Using many-to-many topology (Full):")
        print("  Publisher 1 (port 6001) → Subscribers 1, 2")
        print("  Publisher 2 (port 6002) → Subscribers 1, 3")
        print("  Subscriber 1 receives from both publishers")
    else:
        print_success(f"Custom topology: {num_publishers} publishers → {num_subscribers} subscribers")
        print("  NOTE: Ensure corresponding config files exist for this topology")

    return True


def build_publisher_subscriber_mapping(num_publishers: int, num_subscribers: int) -> dict:
    """Build mapping of which subscribers receive from which publishers
    
    For default 2 publishers, 3 subscribers:
    - Pub 1 → [Sub 1, Sub 2]
    - Pub 2 → [Sub 1, Sub 3]
    
    For custom topologies, assumes overlapping pattern where each subscriber
    receives from at least 2 publishers.
    """
    mapping = {}

    if num_publishers == 2 and num_subscribers == 1:
        # Intermediate A (many-to-one): both publishers → single subscriber
        mapping[1] = [1]  # Pub 1 → Sub 1
        mapping[2] = [1]  # Pub 2 → Sub 1
    elif num_publishers == 2 and num_subscribers == 2:
        # Intermediate B (many-to-two): both publishers → both subscribers
        mapping[1] = [1, 2]  # Pub 1 → Subs 1, 2
        mapping[2] = [1, 2]  # Pub 2 → Subs 1, 2
    elif num_publishers == 2 and num_subscribers == 3:
        # Full many-to-many topology as specified in test plan
        mapping[1] = [1, 2]  # Pub 1 → Subs 1, 2
        mapping[2] = [1, 3]  # Pub 2 → Subs 1, 3
    else:
        # Generic: every publisher sends to every subscriber
        for pub_id in range(1, num_publishers + 1):
            mapping[pub_id] = list(range(1, num_subscribers + 1))

    return mapping


def count_per_publisher(rx_output: str, num_publishers: int) -> dict:
    """Count messages received per publisher by scanning for payload prefixes.

    Returns:
        Dict mapping pub_id (1-based) → message count
    """
    return {
        pub_id: rx_output.count(f"pub{pub_id}_payload")
        for pub_id in range(1, num_publishers + 1)
    }


def validate_payload_attribution(
    rx_outputs: list,
    topology: TopologyTestParameters,
    tx_sent: dict,
    threshold: float
) -> tuple:
    """Validate per-publisher attribution and per-publisher receive threshold.

    For many-to-one each publisher is validated independently:
    - Did the subscriber receive from the right publishers only?
    - Did it receive ≥ threshold % of what each publisher sent?

    Args:
        rx_outputs:  List of RX raw output strings (one per subscriber).
        topology:    Topology with port_mapping (pub_id → [sub_ids]).
        tx_sent:     Dict mapping pub_id → messages sent by that publisher.
        threshold:   Minimum fraction (e.g. 0.80) required per publisher.

    Returns:
        (all_valid: bool, per_sub_per_pub_counts: dict)
        per_sub_per_pub_counts maps sub_id → {pub_id → count}
    """
    print_header("Validating Publisher Attribution")

    # Build reverse mapping: subscriber → expected publishers
    sub_to_pubs: dict = {}
    for pub_id, sub_list in topology.port_mapping.items():
        for sub_id in sub_list:
            sub_to_pubs.setdefault(sub_id, []).append(pub_id)

    all_valid = True
    per_sub_per_pub: dict = {}

    for sub_id, output in enumerate(rx_outputs, start=1):
        expected_pubs = sub_to_pubs.get(sub_id, [])
        counts = count_per_publisher(output, topology.num_publishers)
        per_sub_per_pub[sub_id] = counts

        print(f"\nSubscriber {sub_id}:")
        print(f"  Expected from publishers: {expected_pubs}")

        for pub_id in range(1, topology.num_publishers + 1):
            received = counts[pub_id]
            sent = tx_sent.get(pub_id, 0)
            rate = (received / sent * 100.0) if sent > 0 else 0.0
            min_req = int(sent * threshold)

            print(f"  Pub{pub_id}: received {received}/{sent}  ({rate:.1f}%,  min={min_req})")

            if pub_id in expected_pubs:
                if received == 0:
                    print_error(f"    ERROR: Expected messages from Pub{pub_id} but received none")
                    all_valid = False
                elif received < min_req:
                    print_error(f"    BELOW THRESHOLD: need {min_req}, got {received}")
                    all_valid = False
                else:
                    print_success(f"    OK")
            else:
                if received > 0:
                    print_error(f"    ERROR: Received {received} messages from Pub{pub_id} but not configured")
                    all_valid = False

    return all_valid, per_sub_per_pub


def main():
    """ANO many-to-many test entry point"""
    # Parse arguments
    args = parse_arguments()
    
    # Calculate total containers needed
    total_containers = args.num_publishers + args.num_subscribers
    
    # Validate environment (NICs, license, hugepages)
    if not validate_environment(total_containers):
        print_error("Environment validation failed")
        sys.exit(1)
    
    # Validate topology parameters
    if not validate_topology(args.num_publishers, args.num_subscribers):
        print_error("Topology validation failed")
        sys.exit(1)
    
    # Build publisher-subscriber mapping
    pub_sub_mapping = build_publisher_subscriber_mapping(args.num_publishers, args.num_subscribers)
    
    # Create topology configuration
    topology = TopologyTestParameters.for_many_to_many(
        num_publishers=args.num_publishers,
        num_subscribers=args.num_subscribers
    )
    topology.timeout_seconds = args.timeout
    topology.success_threshold = args.threshold
    topology.port_mapping = pub_sub_mapping
    
    # Create orchestrator
    orchestrator = MultiProcessOrchestrator(topology, test_type="many_to_many")
    
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

    # Build tx_sent map for attribution validation
    tx_sent = {pub_id: results.tx_results[pub_id - 1].tx_sent
               for pub_id in range(1, args.num_publishers + 1)}

    # Validate publisher attribution AND per-publisher threshold in one pass
    attribution_ok, per_sub_per_pub = validate_payload_attribution(
        rx_outputs, topology, tx_sent, args.threshold
    )
    if not attribution_ok:
        print_header("✗ TEST FAILED - Attribution or per-publisher threshold not met")
        sys.exit(3)

    # Validate subscriber isolation (no unexpected duplication)
    if not parser.validate_subscriber_isolation(results):
        print_header("✗ TEST FAILED - Message Duplication Detected")
        sys.exit(4)

    # Validate results
    print_header("Validation")

    # Check each publisher sent messages
    for pub_id in range(1, args.num_publishers + 1):
        tx_result = results.tx_results[pub_id - 1]
        if tx_result.tx_sent == 0:
            print_error(f"Publisher {pub_id} sent 0 messages - test failed")
            sys.exit(1)

    # For many-to-one: validate per-publisher reception independently
    # (the combined total is not a meaningful metric when a single RX queue
    #  handles traffic from multiple concurrent senders)
    all_passed = True
    for sub_id in range(1, args.num_subscribers + 1):
        pub_ids_for_sub = [p for p, subs in pub_sub_mapping.items() if sub_id in subs]
        counts = per_sub_per_pub[sub_id]

        print(f"\nSubscriber {sub_id} summary:")
        print(f"  Receives from publishers: {pub_ids_for_sub}")
        sub_ok = True
        for pub_id in pub_ids_for_sub:
            received = counts[pub_id]
            sent = tx_sent[pub_id]
            min_req = int(sent * args.threshold)
            rate = (received / sent * 100.0) if sent > 0 else 0.0
            print(f"  Pub{pub_id}: {received}/{sent}  ({rate:.1f}%,  need ≥{args.threshold*100:.0f}%)")
            if received < min_req:
                sub_ok = False

        if sub_ok:
            print_success(f"  Subscriber {sub_id} PASSED")
        else:
            print_error(f"  Subscriber {sub_id} FAILED")
            all_passed = False
    
    # Summary
    print("\n" + "=" * 80)
    print(f"Overall Results:")
    print(f"  Publishers: {args.num_publishers}")
    for pub_id in range(1, args.num_publishers + 1):
        print(f"    Publisher {pub_id} sent: {tx_sent[pub_id]} messages")
    print(f"  Subscribers: {args.num_subscribers}")
    total_rx = sum(sum(per_sub_per_pub[s].values()) for s in per_sub_per_pub)
    print(f"  Total messages received (all subs, all pubs): {total_rx}")
    print("=" * 80)
    
    if all_passed:
        print_header("✓ TEST PASSED - All subscribers met threshold")
        sys.exit(0)
    else:
        print_header("✗ TEST FAILED - One or more subscribers below threshold")
        sys.exit(2)


if __name__ == "__main__":
    main()
