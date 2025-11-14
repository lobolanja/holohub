# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
# SPDX-License-Identifier: Apache-2.0

enable_language(CXX)
include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/modules")

include(RTIConnextDDS)

set(CONNEXT_LIB_CPP_SOURCES
    src/connext_lib.cpp
    src/dds_hello.cpp
)

add_library(connext_lib STATIC ${CONNEXT_LIB_CPP_SOURCES})
add_library(holoscan::ops::connext_lib ALIAS connext_lib)

target_include_directories(connext_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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

install(DIRECTORY include/
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
