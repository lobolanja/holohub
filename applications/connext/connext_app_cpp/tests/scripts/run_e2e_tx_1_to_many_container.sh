#!/bin/bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Container-based TX application launcher for ANO 1-to-Many end-to-end testing
# This script runs the connext_app in TX mode for 1-to-many topology
#
# Prerequisites:
#   - Build completed: ./holohub build connext_app_cpp
#   - Physical NICs available with PCIe addresses
#   - RTI license file available
#
# Required environment variables:
#   TEST_TX_NIC_PCIE - PCIe address of TX NIC (e.g., "0005:03:00.0")
#   TEST_RX_NIC_PCIE - PCIe address of RX NIC (e.g., "0005:03:00.1")
#
# Optional environment variables:
#   RTI_LICENSE_FILE - Path to RTI license (default: ./rti_license.dat)
#   CONNEXTDDS_ARCH  - Architecture for Connext (default: armv8Linux4gcc7.3.0)

set -e  # Exit on error

# Configuration
RTI_LICENSE_FILE="${RTI_LICENSE_FILE:-./rti_license.dat}"
CONNEXTDDS_ARCH="${CONNEXTDDS_ARCH:-armv8Linux4gcc7.3.0}"
CONFIG_FILE="config/test_ano_tx_1_to_many.yaml"

# Validate environment
if [ -z "$TEST_TX_NIC_PCIE" ]; then
    echo "ERROR: TEST_TX_NIC_PCIE environment variable not set"
    echo "Example: export TEST_TX_NIC_PCIE=0005:03:00.0"
    exit 1
fi

if [ -z "$TEST_RX_NIC_PCIE" ]; then
    echo "ERROR: TEST_RX_NIC_PCIE environment variable not set"
    echo "Example: export TEST_RX_NIC_PCIE=0005:03:00.1"
    exit 1
fi

if [ ! -f "$RTI_LICENSE_FILE" ]; then
    echo "ERROR: RTI license file not found: $RTI_LICENSE_FILE"
    echo "Set RTI_LICENSE_FILE environment variable or place rti_license.dat in current directory"
    exit 1
fi

echo "=================================================="
echo "Connext ANO 1-to-Many E2E Test - TX Container"
echo "=================================================="
echo "TX NIC PCIe: $TEST_TX_NIC_PCIE"
echo "RX NIC PCIe: $TEST_RX_NIC_PCIE"
echo "License:     $RTI_LICENSE_FILE"
echo "Config:      $CONFIG_FILE"
echo "Architecture: $CONNEXTDDS_ARCH"
echo "=================================================="
echo ""
echo "Starting TX application in container..."
echo "Press Ctrl+C to stop"
echo ""

# Convert license file to absolute path for docker volume mount
RTI_LICENSE_ABS=$(readlink -f "$RTI_LICENSE_FILE")

PATH_TO_APP="/workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/cpp"

# Create DPDK runtime directory for TX (1-to-many)
mkdir -p /home/$USER/dpdk_tx_1tm
chmod 777 /home/$USER/dpdk_tx_1tm

# Run TX application in container with DPDK privileges
# Note: No timeout here - the Python test orchestrator will stop the container
./holohub run-container connext_app_cpp \
  --docker-opts="--user root -v $RTI_LICENSE_ABS:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat --cap-add=SYS_ADMIN --cap-add=IPC_LOCK --cap-add=NET_ADMIN --device=/dev/hugepages:/dev/hugepages --ulimit memlock=-1:-1 --privileged -v /dev/hugepages:/dev/hugepages" \
  -- /bin/bash -c "echo '=== Starting connext_app TX Publisher ===' && $PATH_TO_APP/connext_app $PATH_TO_APP/tests/config/test_ano_tx_1_to_many.yaml 2>&1 || echo 'Exit code:' \$?"
