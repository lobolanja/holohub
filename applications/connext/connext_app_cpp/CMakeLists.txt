# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")

find_package(holoscan 3.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

add_executable(connext_sender_receiver
  main.cpp
  connext_common.cpp
)

target_compile_features(connext_sender_receiver PRIVATE cxx_std_17)

# TODO: change the name to connext_demo_app
target_link_libraries(connext_sender_receiver
  PRIVATE
    holoscan::core
    holoscan::ops::connext_ops
    holoscan::ops::connext_lib
)

add_custom_target(connext_sender_receiver_yaml
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/connext_sender.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/connext_receiver.yaml" ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/connext_sender.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/connext_receiver.yaml"
  BYPRODUCTS
    "${CMAKE_CURRENT_BINARY_DIR}/connext_sender.yaml"
    "${CMAKE_CURRENT_BINARY_DIR}/connext_receiver.yaml"
)

add_dependencies(connext_sender_receiver connext_sender_receiver_yaml)

install(
  TARGETS connext_sender_receiver
  DESTINATION examples/connext
  COMPONENT connext-cpp
)

install(
  FILES
    connext_sender.yaml
    connext_receiver.yaml
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
    connext_sender.yaml
    connext_receiver.yaml
  DESTINATION examples/connext
  COMPONENT connext-cppsrc
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)
