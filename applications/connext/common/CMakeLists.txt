# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

# Define the connext_common static library
add_library(connext_common STATIC
  cpp/gpu_direct_network_sender.cpp
  cpp/gpu_direct_network_receiver.cpp
  cpp/internal/cuda_resource_manager.cpp
  cpp/internal/network_utils.cpp
  cpp/internal/packet_builder.cpp
  cpp/internal/packet_burst_manager.cpp
)

# Set include directories (PUBLIC so consumers get them too)
target_include_directories(connext_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/operators/advanced_network/advanced_network/..
)

# Find and link CUDA runtime
find_package(CUDAToolkit REQUIRED)

# Find holoscan for advanced_network
find_package(holoscan 3.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

target_link_libraries(connext_common
  PUBLIC
    CUDA::cudart
    holoscan::core
)

# Set C++ standard
target_compile_features(connext_common PUBLIC cxx_std_17)

# Add tests subdirectory if testing is enabled
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
