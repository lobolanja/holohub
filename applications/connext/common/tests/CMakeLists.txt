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

# Create test executable
add_executable(test_tx_rx_roundtrip
  test_tx_rx_roundtrip.cpp
)

# Link libraries
target_link_libraries(test_tx_rx_roundtrip
  PRIVATE
    connext_common
    holoscan::advanced_network
    GTest::gtest_main
    GTest::gmock
    CUDA::cudart
    CUDA::cuda_driver
)

# Include directories
target_include_directories(test_tx_rx_roundtrip
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Add test to CTest
add_test(NAME test_tx_rx_roundtrip 
         COMMAND test_tx_rx_roundtrip
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Set test properties (mark as integration test, may need special environment)
set_tests_properties(test_tx_rx_roundtrip PROPERTIES
  LABELS "integration"
  TIMEOUT 30
)
