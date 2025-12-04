# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")

find_package(holoscan 3.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

add_executable(connext_sender_receiver
  main.cpp
  connext_common.cpp
  connext_sender.cpp
  connext_receiver.cpp
)

target_compile_features(connext_sender_receiver PRIVATE cxx_std_17)

target_link_libraries(connext_sender_receiver
  PRIVATE
    holoscan::core
    holoscan::ops::connext_ops
    holoscan::ops::connext_lib
)

install(
  TARGETS connext_sender_receiver
  DESTINATION examples/connext
  COMPONENT connext-cpp
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
    connext_sender.cpp
    connext_receiver.cpp
    connext_common.hpp
    connext_common.cpp
  DESTINATION examples/connext
  COMPONENT connext-cppsrc
  PERMISSIONS OWNER_READ OWNER_WRITE
              GROUP_READ GROUP_WRITE
              WORLD_READ WORLD_WRITE
)
