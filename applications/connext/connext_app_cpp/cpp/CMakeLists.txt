# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")

find_package(holoscan 3.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

add_executable(connext_app
  main.cpp
  connext_common.cpp
)

target_compile_features(connext_app PRIVATE cxx_std_17)

# TODO: change the name to connext_demo_app
target_link_libraries(connext_app
  PRIVATE
    holoscan::core
    holoscan::ops::connext_ops
    holoscan::ops::connext_lib
)

add_custom_target(connext_app_yaml
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/../connext_sender.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/../connext_receiver.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../connext_sender.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../connext_receiver.yaml"
  BYPRODUCTS
    "${CMAKE_CURRENT_BINARY_DIR}/connext_sender.yaml"
    "${CMAKE_CURRENT_BINARY_DIR}/connext_receiver.yaml"
)

add_custom_target(connext_app_tests
  # Copy Python test files
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/tests/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_ano_e2e.py" ${CMAKE_CURRENT_BINARY_DIR}/tests
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_dds_e2e.py" ${CMAKE_CURRENT_BINARY_DIR}/tests
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_utils.py" ${CMAKE_CURRENT_BINARY_DIR}/tests
  # Create subdirectories
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/tests/scripts
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/tests/config
  # Copy shell scripts
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_rx_container.sh" ${CMAKE_CURRENT_BINARY_DIR}/tests/scripts
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_tx_container.sh" ${CMAKE_CURRENT_BINARY_DIR}/tests/scripts
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_dds_rx_container.sh" ${CMAKE_CURRENT_BINARY_DIR}/tests/scripts
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_dds_tx_container.sh" ${CMAKE_CURRENT_BINARY_DIR}/tests/scripts
  # Copy YAML config files
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_ano_rx.yaml" ${CMAKE_CURRENT_BINARY_DIR}/tests/config
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_ano_tx.yaml" ${CMAKE_CURRENT_BINARY_DIR}/tests/config
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_dds_rx.yaml" ${CMAKE_CURRENT_BINARY_DIR}/tests/config
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_dds_tx.yaml" ${CMAKE_CURRENT_BINARY_DIR}/tests/config
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_ano_e2e.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_dds_e2e.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_utils.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_rx_container.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_tx_container.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_dds_rx_container.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/run_e2e_dds_tx_container.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_ano_rx.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_ano_tx.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_dds_rx.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/config/test_dds_tx.yaml"
)


add_dependencies(connext_app connext_app_yaml connext_app_tests)

install(
  TARGETS connext_app
  DESTINATION examples/connext
  COMPONENT connext-cpp
)

install(
  FILES
    ../connext_sender.yaml
    ../connext_receiver.yaml
  DESTINATION examples/connext
  COMPONENT connext-cpp
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

install(
  FILES CMakeLists.txt.install
  RENAME CMakeLists.txt
  DESTINATION examples/connext
  COMPONENT connext-cppsrc
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)

install(
  FILES
    main.cpp
    connext_common.hpp
    connext_common.cpp
    ../connext_sender.yaml
    ../connext_receiver.yaml
  DESTINATION examples/connext
  COMPONENT connext-cppsrc
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)
