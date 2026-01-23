# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ============================================================================
# Unit Tests (no external dependencies - pure logic)
# ============================================================================

add_executable(connext_ano_unit_tests
  unit/test_config_validation.cpp
  unit/test_network_utils.cpp
  unit/test_packet_builder.cpp
)

target_link_libraries(connext_ano_unit_tests
  PRIVATE
    connext_ano_lib
    GTest::gtest_main
)

target_include_directories(connext_ano_unit_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_test(NAME connext_ano_unit_tests
         COMMAND connext_ano_unit_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(connext_ano_unit_tests PROPERTIES
  LABELS "unit"
  TIMEOUT 10
)

# ============================================================================
# Component Tests (require CUDA runtime)
# ============================================================================

add_executable(connext_ano_component_tests
  component/test_cuda_resource_manager.cpp
  component/test_cuda_raii_wrappers.cpp
)

target_link_libraries(connext_ano_component_tests
  PRIVATE
    connext_ano_lib
    GTest::gtest_main
    CUDA::cudart
)

target_include_directories(connext_ano_component_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_test(NAME connext_ano_component_tests
         COMMAND connext_ano_component_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(connext_ano_component_tests PROPERTIES
  LABELS "component;requires_gpu"
  TIMEOUT 30
)

# ============================================================================
# Integration Tests (require CUDA + DPDK + Advanced Network)
# ============================================================================

# Test 1: Loopback test (software loopback, no hardware required)
add_executable(test_tx_rx_loopback
  integration/test_tx_rx_roundtrip.cpp
)

target_link_libraries(test_tx_rx_loopback
  PRIVATE
    connext_ano_lib
    holoscan::advanced_network
    GTest::gtest_main
    GTest::gmock
    CUDA::cudart
    CUDA::cuda_driver
)

target_include_directories(test_tx_rx_loopback
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

target_compile_definitions(test_tx_rx_loopback
  PRIVATE
    TEST_LOOPBACK_ONLY
)

add_test(NAME test_tx_rx_loopback 
         COMMAND test_tx_rx_loopback
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(test_tx_rx_loopback PROPERTIES
  LABELS "integration;loopback"
  TIMEOUT 30
)

# Test 2: Physical NIC test (requires two physical NICs connected via cable)
add_executable(test_tx_rx_physical
  integration/test_tx_rx_roundtrip.cpp
)

target_link_libraries(test_tx_rx_physical
  PRIVATE
    connext_ano_lib
    holoscan::advanced_network
    GTest::gtest_main
    GTest::gmock
    CUDA::cudart
    CUDA::cuda_driver
)

target_include_directories(test_tx_rx_physical
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

target_compile_definitions(test_tx_rx_physical
  PRIVATE
    TEST_PHYSICAL_ONLY
)

add_test(NAME test_tx_rx_physical 
         COMMAND test_tx_rx_physical
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(test_tx_rx_physical PROPERTIES
  LABELS "integration;requires_hardware"
  TIMEOUT 30
)
