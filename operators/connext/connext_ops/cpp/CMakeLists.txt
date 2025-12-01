# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

enable_language(CXX)
include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/modules")

include(RTIConnextDDS)
find_package(holoscan 3.0 REQUIRED CONFIG
             PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")

set(CONNEXT_OPS_SOURCES
    src/connext_rx.cpp
    src/connext_tx.cpp
)

add_library(connext_ops STATIC ${CONNEXT_OPS_SOURCES})
add_library(holoscan::ops::connext_ops ALIAS connext_ops)

target_include_directories(connext_ops
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/connext_ops>
)

target_compile_features(connext_ops PUBLIC cxx_std_17)

target_link_libraries(connext_ops
  PUBLIC
    holoscan::core
    holoscan::ops::connext_lib
)
# connext_ops wraps helpers built in connext_lib, so ensure the static library is
# produced before we attempt to build these operators.
add_dependencies(connext_ops connext_lib)

install(TARGETS connext_ops
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/connext_ops)

if(BUILD_TESTING)
  enable_testing()
  add_executable(test_connext_rx_integration tests/test_connext_rx_integration.cpp)
  target_link_libraries(test_connext_rx_integration
    PRIVATE
      connext_ops
      holoscan::core
      holoscan::ops::connext_lib
      RTIConnextDDS::test
  )
  add_test(NAME test_connext_rx_integration COMMAND test_connext_rx_integration)

  add_custom_target(connext_ops_cpp_tests
          DEPENDS
          test_connext_rx_integration
  )
  set(CONNEXT_LIB_TEST_ENV "")
  if(DEFINED ENV{NDDSHOME})
    list(APPEND CONNEXT_LIB_TEST_ENV "NDDSHOME=$ENV{NDDSHOME}")
  endif()
  if(DEFINED ENV{RTI_LICENSE_FILE})
    list(APPEND CONNEXT_LIB_TEST_ENV "RTI_LICENSE_FILE=$ENV{RTI_LICENSE_FILE}")
  endif()
  if(CONNEXT_LIB_TEST_ENV)
    set_tests_properties(
            test_connext_rx_integration PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
  endif()
endif()
