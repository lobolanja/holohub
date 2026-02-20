#!/usr/bin/env python3
"""
Simplified ANO End-to-End Container Test

Rate-based testing with timeout control:
- TX sends at 10 msg/sec for 30 seconds (~300 messages)
- RX receives for 30 seconds
- Test validates that RX received at least 80% of TX messages

Prerequisites:
    - Physical NICs with TEST_TX_NIC_PCIE and TEST_RX_NIC_PCIE environment variables
    - RTI license file (RTI_LICENSE_FILE or ./rti_license.dat)
    - NICs connected via cable or network switch

Usage:
    export TEST_TX_NIC_PCIE="0005:03:00.0"
    export TEST_RX_NIC_PCIE="0005:03:00.1"
    export RTI_LICENSE_FILE="./rti_license.dat"
    
    cd /workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/tests
    python3 test_ano_e2e.py
"""

import sys
from test_utils import (
    TestScriptPaths,
    ANOEnvironmentValidator,
    ANOOutputParser,
    TestParameters,
    E2ETestRunner,
    print_header
)


def main():
    """ANO end-to-end test entry point"""
    # Configure for ANO transport
    paths = TestScriptPaths.for_ano()
    validator = ANOEnvironmentValidator(paths)
    parser = ANOOutputParser()
    params = TestParameters(rx_init_wait=5)  # ANO initialization time
    
    # Run test
    runner = E2ETestRunner(paths, validator, parser, params)
    
    if not runner.validate_environment():
        sys.exit(1)
    
    success = runner.run_test()
    
    if success:
        print_header("✓ TEST PASSED")
        sys.exit(0)
    else:
        print_header("✗ TEST FAILED")
        sys.exit(1)


if __name__ == "__main__":
    main()
