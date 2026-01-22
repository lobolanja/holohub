# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(connext_ano_basic_app CXX CUDA)


find_package(holoscan 3.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")
# Find CUDA
find_package(CUDAToolkit REQUIRED)

# Create executables
add_executable(connext_ano_basic_app
  cpp/main.cpp
  cpp/tensor_generator_op.cpp
  cpp/tensor_printer_op.cpp
  cpp/tensor_network_tx_op.cpp
  cpp/tensor_network_rx_op.cpp
  cpp/demo_ano_tx_app.cpp
  cpp/demo_ano_rx_app.cpp
)

# Combined TX+RX in single DPDK process
add_executable(demo_ano_tx_rx
  cpp/demo_ano_tx_rx.cpp
  cpp/tensor_generator_op.cpp
  cpp/tensor_network_tx_op.cpp
  cpp/tensor_network_rx_op.cpp
  cpp/tensor_printer_op.cpp
)

target_compile_features(connext_ano_basic_app PRIVATE cxx_std_17)
target_compile_features(demo_ano_tx_rx PRIVATE cxx_std_17)

target_include_directories(connext_ano_basic_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(demo_ano_tx_rx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(connext_ano_basic_app
  PRIVATE
    holoscan::core
    holoscan::advanced_network
    CUDA::cudart
    CUDA::cuda_driver
    connext_common
)

target_link_libraries(demo_ano_tx_rx
  PRIVATE
    holoscan::core
    holoscan::advanced_network
    CUDA::cudart
    CUDA::cuda_driver
    connext_common
)

set(ANO_MGR "dpdk" CACHE STRING "Manager type(s) list")
separate_arguments(ANO_MGR_LIST UNIX_COMMAND ${ANO_MGR})
foreach(MGR IN LISTS ANO_MGR_LIST)
    message(STATUS "Adding manager ${MGR} to the configuration")
    target_compile_definitions(connext_ano_basic_app PUBLIC "ANO_MGR_${MGR}=1")
    target_compile_definitions(demo_ano_tx_rx PUBLIC "ANO_MGR_${MGR}=1")
endforeach()


# Copy config files to build directory
add_custom_target(connext_ano_basic_app_yaml
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_tx.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_rx.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_tx_rx.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_tx_rx_loopback.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_tx.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_rx.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_tx_rx.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/demo_ano_tx_rx_loopback.yaml"
  BYPRODUCTS
    "${CMAKE_CURRENT_BINARY_DIR}/demo_ano_tx.yaml"
    "${CMAKE_CURRENT_BINARY_DIR}/demo_ano_rx.yaml"
    "${CMAKE_CURRENT_BINARY_DIR}/demo_ano_tx_rx.yaml"
    "${CMAKE_CURRENT_BINARY_DIR}/demo_ano_tx_rx_loopback.yaml"
)

add_dependencies(connext_ano_basic_app connext_ano_basic_app_yaml)
add_dependencies(demo_ano_tx_rx connext_ano_basic_app_yaml)

# Install targets
install(
  TARGETS connext_ano_basic_app
  DESTINATION examples/connext
  COMPONENT connext-ano-basic
)

install(
  TARGETS demo_ano_tx_rx
  DESTINATION examples/connext
  COMPONENT demo-ano-tx-rx
)

install(
  FILES
    demo_ano_tx.yaml
    demo_ano_rx.yaml
  DESTINATION examples/connext
  COMPONENT connext-ano-basic
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

install(
  FILES
    demo_ano_tx_rx.yaml
    demo_ano_tx_rx_loopback.yaml
  DESTINATION examples/connext
  COMPONENT demo-ano-tx-rx
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

install(
  FILES CMakeLists.txt.install
  RENAME CMakeLists.txt
  DESTINATION examples/connext
  COMPONENT connext-ano-basicsrc
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

install(
  FILES
    demo_ano_tx.yaml
    demo_ano_rx.yaml
  DESTINATION examples/connext
  COMPONENT connext-ano-basicsrc
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

install(
  DIRECTORY cpp/
  DESTINATION examples/connext/cpp
  COMPONENT connext-ano-basicsrc
  FILES_MATCHING PATTERN "*.cpp"
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

install(
  DIRECTORY include/
  DESTINATION examples/connext/include
  COMPONENT connext-ano-basicsrc
  FILES_MATCHING PATTERN "*.h"
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

# Add testing
if(BUILD_TESTING)
  # Copy the test files to the build directory
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/conftest.py" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests")
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/process_utils.py" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests")
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_demo_ano.py" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests")
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_ano_tx_rx_loopback_test.yaml" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests")

  # Register individual pytest tests with CTest
  include(add_python_tests)
  add_python_tests(
    INPUT "tests"
    PYTEST_ARGS "-v" "-s" "--log-cli-level=DEBUG" "--show-capture=no"
  )

endif()
