#!/bin/bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Diagnostic script to check GPU and DPDK state before running tests

echo "=== GPU State ==="
nvidia-smi --query-gpu=index,name,memory.used,memory.free --format=csv

echo ""
echo "=== DPDK Hugepages ==="
cat /sys/devices/system/node/node*/hugepages/hugepages-1048576kB/nr_hugepages
echo "Free hugepages:"
cat /sys/devices/system/node/node*/hugepages/hugepages-1048576kB/free_hugepages

echo ""
echo "=== Running Docker Containers ==="
docker ps | grep connext_app_cpp || echo "No connext_app_cpp containers running"

echo ""
echo "=== DPDK Runtime Directories ==="
ls -la /home/$USER/dpdk_* 2>/dev/null || echo "No DPDK runtime directories found"

echo ""
echo "=== NIC Status ==="
if [ -n "$TEST_TX_NIC_PCIE" ]; then
    echo "TX NIC ($TEST_TX_NIC_PCIE):"
    lspci -vvv -s $TEST_TX_NIC_PCIE | grep -E "LnkSta:|Device Serial Number" || echo "Not found"
fi

if [ -n "$TEST_RX_NIC_PCIE" ]; then
    echo "RX NIC ($TEST_RX_NIC_PCIE):"
    lspci -vvv -s $TEST_RX_NIC_PCIE | grep -E "LnkSta:|Device Serial Number" || echo "Not found"
fi

echo ""
echo "=== GPU DMA Capability ==="
if command -v nvidia-smi &> /dev/null; then
    nvidia-smi topo -m | head -20
fi
