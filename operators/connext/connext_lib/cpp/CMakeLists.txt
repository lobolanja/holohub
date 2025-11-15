# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
# SPDX-License-Identifier: Apache-2.0

enable_language(CXX)
include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/modules")

include(RTIConnextDDS)

set(CONNEXT_LIB_CPP_SOURCES
    src/connext_lib.cpp
    src/dds_hello.cpp
    src/config.cpp
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

  add_executable(connext_lib_dds_tests tests/dds_tests.cpp)
  target_link_libraries(connext_lib_dds_tests PRIVATE connext_lib RTIConnextDDS::test)
  add_test(NAME connext_lib_dds_tests COMMAND connext_lib_dds_tests)

  add_executable(connext_lib_config_tests tests/config_tests.cpp)
  target_link_libraries(connext_lib_config_tests PRIVATE connext_lib RTIConnextDDS::test)
  add_test(NAME connext_lib_config_tests COMMAND connext_lib_config_tests)

  set(CONNEXT_LIB_TEST_ENV "")
  if(DEFINED ENV{NDDSHOME})
    list(APPEND CONNEXT_LIB_TEST_ENV "NDDSHOME=$ENV{NDDSHOME}")
  endif()
  if(DEFINED ENV{RTI_LICENSE_FILE})
    list(APPEND CONNEXT_LIB_TEST_ENV "RTI_LICENSE_FILE=$ENV{RTI_LICENSE_FILE}")
  endif()
  if(CONNEXT_LIB_TEST_ENV)
    set_tests_properties(
      connext_lib_dds_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    set_tests_properties(
      connext_lib_config_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
  endif()
endif()
