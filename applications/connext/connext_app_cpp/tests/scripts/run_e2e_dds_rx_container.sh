#!/bin/bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Container-based RX application launcher for DDS end-to-end testing
# This script runs the connext_sender_receiver app in RX mode with DDS transport
#
# Prerequisites:
#   - Build completed: ./holohub build connext_app_cpp
#   - RTI license file available
#
# Optional environment variables:
#   RTI_LICENSE_FILE - Path to RTI license (default: ./rti_license.dat)
#   CONNEXTDDS_ARCH  - Architecture for Connext (default: armv8Linux4gcc7.3.0)

set -e  # Exit on error

# Configuration
RTI_LICENSE_FILE="${RTI_LICENSE_FILE:-./rti_license.dat}"
CONNEXTDDS_ARCH="${CONNEXTDDS_ARCH:-armv8Linux4gcc7.3.0}"
CONFIG_FILE="config/test_dds_rx.yaml"
PATH_TO_APP="/workspace/holohub/build/connext_app_cpp/applications/connext/connext_app_cpp/cpp"

# Validate environment
if [ ! -f "$RTI_LICENSE_FILE" ]; then
    echo "ERROR: RTI license file not found: $RTI_LICENSE_FILE"
    echo "Set RTI_LICENSE_FILE environment variable or place rti_license.dat in current directory"
    exit 1
fi

echo "=================================================="
echo "Connext DDS E2E Test - RX Container"
echo "=================================================="
echo "License:      $RTI_LICENSE_FILE"
echo "Config:       $CONFIG_FILE"
echo "Architecture: $CONNEXTDDS_ARCH"
echo "Transport:    DDS (domain_id=42, topic=testing_dds_topic)"
echo "=================================================="
echo ""
echo "Starting RX application in container..."
echo "Press Ctrl+C to stop"
echo ""

# Convert license file to absolute path for docker volume mount
RTI_LICENSE_ABS=$(readlink -f "$RTI_LICENSE_FILE")

# Run RX application in container
# DDS doesn't need privileged mode or DPDK setup (network=host is added by run-container)
./holohub run-container connext_app_cpp \
  --docker-opts="--user root -v $RTI_LICENSE_ABS:/opt/rti.com/rti_connext_dds-7.3.0/rti_license.dat -e CONNEXTDDS_ARCH=$CONNEXTDDS_ARCH -e CONNEXT_CONTAINER_MODE=1 -w $PATH_TO_APP" \
  -- /bin/bash -c "echo '=== Starting connext_app (DDS RX) ===' && $PATH_TO_APP/connext_app $PATH_TO_APP/tests/config/test_dds_rx.yaml 2>&1 || echo 'Exit code:' \$?"