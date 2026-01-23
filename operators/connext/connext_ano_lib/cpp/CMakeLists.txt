# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

enable_language(CXX)
include(GNUInstallDirs)

# Define the connext_ano_lib static library
add_library(connext_ano_lib STATIC
  src/gpu_direct_network_sender.cpp
  src/gpu_direct_network_receiver.cpp
  src/internal/cuda_resource_manager.cpp
  src/internal/network_utils.cpp
  src/internal/packet_builder.cpp
  src/internal/packet_burst_manager.cpp
)

# Add alias for consistency with other connext libraries
add_library(holoscan::ops::connext_ano_lib ALIAS connext_ano_lib)

# Set include directories (PUBLIC so consumers get them too)
target_include_directories(connext_ano_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/connext_ano_lib>
    ${CMAKE_SOURCE_DIR}/operators/advanced_network/advanced_network/..
)

# Find and link CUDA runtime
find_package(CUDAToolkit REQUIRED)

# Find holoscan for advanced_network
find_package(holoscan 3.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

target_link_libraries(connext_ano_lib
  PUBLIC
    CUDA::cudart
    holoscan::core
)

# Set C++ standard
target_compile_features(connext_ano_lib PUBLIC cxx_std_17)

# Install library
install(TARGETS connext_ano_lib
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/connext_ano_lib
        FILES_MATCHING PATTERN "*.h")

# Add tests subdirectory if testing is enabled
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
