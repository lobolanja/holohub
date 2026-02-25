#!/bin/bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Container-based RX application launcher for ANO Many-to-Many end-to-end testing
# This script runs the connext_app in RX mode for a specific subscriber
#
# Usage:
#   ./run_e2e_rx_many_to_many_container.sh <subscriber_id>
#   where subscriber_id is 1, 2, 3, or 4
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

# Validate subscriber ID parameter
if [ -z "$1" ]; then
    echo "ERROR: Subscriber ID parameter required"
    echo "Usage: $0 <subscriber_id>"
    echo "  subscriber_id: 1, 2, or 3"
    exit 1
fi

SUBSCRIBER_ID="$1"

# Validate subscriber ID range
if [[ ! "$SUBSCRIBER_ID" =~ ^[1-3]$ ]]; then
    echo "ERROR: Invalid subscriber ID: $SUBSCRIBER_ID"
    echo "Subscriber ID must be 1, 2, or 3"
    exit 1
fi

# Configuration
RTI_LICENSE_FILE="${RTI_LICENSE_FILE:-./rti_license.dat}"
CONNEXTDDS_ARCH="${CONNEXTDDS_ARCH:-armv8Linux4gcc7.3.0}"
CONFIG_FILE="config/test_ano_rx_many_to_many_sub${SUBSCRIBER_ID}.yaml"

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

# Path to application inside container
PATH_TO_APP="/workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/cpp"

# Note: Config file validation will happen inside container
# The file should exist at: $PATH_TO_APP/tests/$CONFIG_FILE

echo "=================================================="
echo "Connext ANO Many-to-Many E2E Test - RX Container"
echo "Subscriber ${SUBSCRIBER_ID}"
echo "=================================================="
echo "TX NIC PCIe: $TEST_TX_NIC_PCIE"
echo "RX NIC PCIe: $TEST_RX_NIC_PCIE"
echo "License:     $RTI_LICENSE_FILE"
echo "Config:      $CONFIG_FILE"
echo "UDP Port:    600${SUBSCRIBER_ID}"
echo "Architecture: $CONNEXTDDS_ARCH"
echo "=================================================="
echo ""
echo "Starting RX application in container..."
echo "Press Ctrl+C to stop"
echo ""

# Convert license file to absolute path for docker volume mount
RTI_LICENSE_ABS=$(readlink -f "$RTI_LICENSE_FILE")

# Create unique DPDK runtime directory for this RX subscriber
mkdir -p /home/$USER/dpdk_rx_mtm_sub${SUBSCRIBER_ID}
chmod 777 /home/$USER/dpdk_rx_mtm_sub${SUBSCRIBER_ID}

# Run RX application in container with DPDK privileges
# Note: No timeout here - the Python test orchestrator will stop the container
./holohub run-container connext_app_cpp \
  --docker-opts="--user root -v $RTI_LICENSE_ABS:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat --cap-add=SYS_ADMIN --cap-add=IPC_LOCK --cap-add=NET_ADMIN --device=/dev/hugepages:/dev/hugepages --ulimit memlock=-1:-1 --privileged -v /dev/hugepages:/dev/hugepages" \
  -- /bin/bash -c "echo '=== Starting connext_app RX Subscriber ${SUBSCRIBER_ID} ===' && $PATH_TO_APP/connext_app $PATH_TO_APP/tests/config/test_ano_rx_many_to_many_sub${SUBSCRIBER_ID}.yaml 2>&1 || echo 'Exit code:' \$?"
