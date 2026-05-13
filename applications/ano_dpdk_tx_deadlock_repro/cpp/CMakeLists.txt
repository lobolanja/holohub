# SPDX-FileCopyrightText: Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(ano_dpdk_tx_deadlock_repro CXX)

find_package(holoscan 2.6 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

add_executable(ano_dpdk_tx_deadlock_repro
  main.cpp
)

target_link_libraries(ano_dpdk_tx_deadlock_repro
  PRIVATE
  holoscan::core
  holoscan::advanced_network
)

# This reproducer only exercises the DPDK backend.
target_compile_definitions(ano_dpdk_tx_deadlock_repro PUBLIC ANO_MGR_DPDK=1)

# Copy configs next to the binary AND to the holohub build root so they can be
# run from either location (the metadata.json declares workdir = holohub_bin
# which resolves to the top-level build dir).
add_custom_target(ano_dpdk_tx_deadlock_repro_yaml ALL
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/../ano_dpdk_tx_deadlock_repro.yaml"
    ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/../ano_dpdk_tx_deadlock_repro.yaml"
    ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/../ano_dpdk_tx_deadlock_repro_no_repro.yaml"
    ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/../ano_dpdk_tx_deadlock_repro_no_repro.yaml"
    ${CMAKE_BINARY_DIR}
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../ano_dpdk_tx_deadlock_repro.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ano_dpdk_tx_deadlock_repro_no_repro.yaml"
)
add_dependencies(ano_dpdk_tx_deadlock_repro ano_dpdk_tx_deadlock_repro_yaml)
