# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

cmake_minimum_required(VERSION 3.20)
project(connext_lib LANGUAGES NONE)

option(CONNEXT_LIB_BUILD_CPP "Build the native Connext helper library" ON)

if(CONNEXT_LIB_BUILD_CPP)
  enable_language(CXX)
  include(GNUInstallDirs)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")

  include(RTIConnextDDS)

  set(CONNEXT_LIB_CPP_SOURCES
      cpp/src/connext_lib.cpp
      cpp/src/dds_hello.cpp
  )

  add_library(connext_lib STATIC ${CONNEXT_LIB_CPP_SOURCES})
  add_library(holoscan::ops::connext_lib ALIAS connext_lib)

  target_include_directories(connext_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/connext_lib>
  )

  target_compile_features(connext_lib PUBLIC cxx_std_17)

  target_link_libraries(connext_lib PUBLIC RTIConnextDDS::cpp2_api)
  target_compile_definitions(connext_lib PUBLIC CONNEXT_LIB_HAS_RTI=1)

  install(TARGETS connext_lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(DIRECTORY cpp/include/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/connext_lib
          FILES_MATCHING PATTERN "*.hpp")

  if(BUILD_TESTING)
    enable_testing()
    add_executable(connext_lib_version_test tests/connext_lib_version_test.cpp)
    target_link_libraries(connext_lib_version_test PRIVATE connext_lib)
    add_test(NAME connext_lib_version_test COMMAND connext_lib_version_test)

    add_executable(connext_lib_dds_hello_test tests/connext_lib_dds_hello_test.cpp)
    target_link_libraries(connext_lib_dds_hello_test PRIVATE connext_lib)
    add_test(NAME connext_lib_dds_hello_test COMMAND connext_lib_dds_hello_test)
  endif()
endif()

install(FILES metadata.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/holohub/operators/connext)

if(HOLOHUB_BUILD_PYTHON)
  add_subdirectory(python)
endif()
