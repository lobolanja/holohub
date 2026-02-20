#!/usr/bin/env python3
"""
DDS End-to-End Container Test

Rate-based testing with timeout control for DDS transport:
- TX sends at 10 msg/sec for 30 seconds (~300 messages)
- RX receives for 30 seconds
- Test validates that RX received at least 80% of TX messages

This test is parallel to test_ano_e2e.py but uses DDS transport
instead of ANO (Advanced Network Operator).

Prerequisites:
    - RTI license file (RTI_LICENSE_FILE or ./rti_license.dat)
    - Containers can communicate via network (uses --network=host)

Usage:
    export RTI_LICENSE_FILE="./rti_license.dat"
    
    cd /workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/tests
    python3 test_dds_e2e.py
"""

import sys
from test_utils import (
    TestScriptPaths,
    DDSEnvironmentValidator,
    DDSOutputParser,
    TestParameters,
    E2ETestRunner,
    print_header
)


def main():
    """DDS end-to-end test entry point"""
    # Configure for DDS transport
    paths = TestScriptPaths.for_dds()
    validator = DDSEnvironmentValidator(paths)
    parser = DDSOutputParser()
    params = TestParameters(rx_init_wait=3)  # DDS initialization time
    
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
