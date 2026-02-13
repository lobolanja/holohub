# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
# SPDX-License-Identifier: Apache-2.0

enable_language(CXX)
include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/modules")

# Generate IDL type library for ResourceAnnouncement
add_rti_type_library(ResourceAnnouncement ${CMAKE_CURRENT_SOURCE_DIR}/../idl/ResourceAnnouncement.idl)

set(CONNEXT_LIB_CPP_SOURCES
    src/config/config.cpp
    src/comm/connext_comm.cpp
    src/comm/connext_readers.cpp
    src/comm/connext_writers.cpp
    src/transport/payload_transport_dds.cpp
    src/transport/payload_transport_ano.cpp
    src/transport/sender_factory.cpp
    src/resource/resource_managers.cpp
    src/resource/resource_managers_dds.cpp
    src/resource/resource_managers_idl.cpp
)

add_library(connext_lib STATIC ${CONNEXT_LIB_CPP_SOURCES})
add_library(holoscan::ops::connext_lib ALIAS connext_lib)

target_include_directories(connext_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../connext_ano_lib/cpp/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/connext_lib>
)

target_compile_features(connext_lib PUBLIC cxx_std_17)

target_link_libraries(connext_lib PUBLIC RTIConnextDDS::cpp2_api ResourceAnnouncement)
target_link_libraries(connext_lib PRIVATE connext_ano_lib)
target_compile_definitions(connext_lib PUBLIC CONNEXT_LIB_HAS_RTI=1)

# Make ResourceAnnouncement include paths available
target_include_directories(connext_lib PUBLIC
  $<TARGET_PROPERTY:ResourceAnnouncement,INTERFACE_INCLUDE_DIRECTORIES>
)

# Make connext_ano_lib include paths available to all consumers
target_include_directories(connext_lib PUBLIC
  $<TARGET_PROPERTY:connext_ano_lib,INTERFACE_INCLUDE_DIRECTORIES>
)

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
  find_package(CUDAToolkit REQUIRED)

  add_executable(connext_lib_config_tests tests/config_tests.cpp)
  target_link_libraries(connext_lib_config_tests PRIVATE connext_lib RTIConnextDDS::test)
  add_test(NAME connext_lib_config_tests COMMAND connext_lib_config_tests)

  # payload transport interface unit test removed (uses fake implementations).

  add_executable(connext_lib_payload_transport_dds_tests tests/payload_transport_dds_tests.cpp)
  target_link_libraries(connext_lib_payload_transport_dds_tests PRIVATE connext_lib RTIConnextDDS::test RTIConnextDDS::test_helpers)
  add_test(NAME connext_lib_payload_transport_dds_tests COMMAND connext_lib_payload_transport_dds_tests)

  add_executable(connext_lib_payload_transport_ano_writer_tests tests/payload_transport_ano_writer_tests.cpp)
  target_link_libraries(connext_lib_payload_transport_ano_writer_tests PRIVATE connext_lib RTIConnextDDS::test RTIConnextDDS::test_helpers connext_ano_lib)
  target_include_directories(connext_lib_payload_transport_ano_writer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../connext_ano_lib/cpp/include)
  add_test(NAME connext_lib_payload_transport_ano_writer_tests COMMAND connext_lib_payload_transport_ano_writer_tests)
  
  add_executable(connext_lib_payload_transport_ano_reader_tests tests/payload_transport_ano_reader_tests.cpp)
  target_link_libraries(connext_lib_payload_transport_ano_reader_tests PRIVATE connext_lib RTIConnextDDS::test RTIConnextDDS::test_helpers connext_ano_lib)
  target_include_directories(connext_lib_payload_transport_ano_reader_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../connext_ano_lib/cpp/include)
  add_test(NAME connext_lib_payload_transport_ano_reader_tests COMMAND connext_lib_payload_transport_ano_reader_tests)

  add_executable(connext_lib_resource_managers_dds_tests tests/resource_managers_dds_tests.cpp)
  target_link_libraries(connext_lib_resource_managers_dds_tests PRIVATE connext_lib RTIConnextDDS::test)
  add_test(NAME connext_lib_resource_managers_dds_tests COMMAND connext_lib_resource_managers_dds_tests)

  add_executable(connext_lib_resource_managers_idl_tests tests/resource_managers_idl_tests.cpp)
  target_link_libraries(connext_lib_resource_managers_idl_tests PRIVATE connext_lib RTIConnextDDS::test ResourceAnnouncement)
  add_test(NAME connext_lib_resource_managers_idl_tests COMMAND connext_lib_resource_managers_idl_tests)

  add_executable(connext_lib_comm_tests tests/connext_comm_tests.cpp)
  target_link_libraries(connext_lib_comm_tests PRIVATE connext_lib RTIConnextDDS::test RTIConnextDDS::test_helpers)
  add_test(NAME connext_lib_comm_tests COMMAND connext_lib_comm_tests)

  add_executable(connext_lib_ano_integration_tests tests/connext_ano_integration_tests.cpp)
  target_link_libraries(connext_lib_ano_integration_tests PRIVATE connext_lib connext_ano_lib holoscan::advanced_network RTIConnextDDS::test CUDA::cudart)
  add_test(NAME connext_lib_ano_integration_tests COMMAND connext_lib_ano_integration_tests)

  add_executable(connext_lib_dds_integration_tests tests/connext_dds_integration_tests.cpp)
  target_link_libraries(connext_lib_dds_integration_tests PRIVATE connext_lib RTIConnextDDS::test RTIConnextDDS::test_helpers)
  add_test(NAME connext_lib_dds_integration_tests COMMAND connext_lib_dds_integration_tests)

  add_executable(connext_lib_public_api_tests tests/connext_lib_public_api_tests.cpp)
  target_link_libraries(connext_lib_public_api_tests PRIVATE connext_lib RTIConnextDDS::test CUDA::cudart)
  add_test(NAME connext_lib_public_api_tests COMMAND connext_lib_public_api_tests)

  add_custom_target(connext_lib_cpp_tests
    DEPENDS
      connext_lib_dds_tests
      connext_lib_config_tests
      # connext_lib_payload_transport_tests (removed)
      connext_lib_payload_transport_dds_tests
      connext_lib_payload_transport_ano_tests
      connext_lib_payload_transport_ano_reader_tests
      connext_lib_resource_managers_dds_tests
      connext_lib_resource_managers_idl_tests
      connext_lib_comm_tests
      connext_lib_ano_integration_tests
      connext_lib_dds_integration_tests
      connext_lib_public_api_tests)

  set(CONNEXT_LIB_TEST_ENV "")
  if(DEFINED ENV{NDDSHOME})
    list(APPEND CONNEXT_LIB_TEST_ENV "NDDSHOME=$ENV{NDDSHOME}")
  endif()
  if(DEFINED ENV{RTI_LICENSE_FILE})
    list(APPEND CONNEXT_LIB_TEST_ENV "RTI_LICENSE_FILE=$ENV{RTI_LICENSE_FILE}")
  endif()
  if(CONNEXT_LIB_TEST_ENV)
    set_tests_properties(
      connext_lib_config_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    # connext_lib_payload_transport_tests properties removed
    set_tests_properties(
      connext_lib_payload_transport_dds_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    set_tests_properties(
      connext_lib_resource_managers_dds_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    set_tests_properties(
      connext_lib_resource_managers_idl_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    set_tests_properties(
      connext_lib_comm_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    set_tests_properties(
      connext_lib_ano_integration_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    set_tests_properties(
      connext_lib_dds_integration_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
    set_tests_properties(
      connext_lib_public_api_tests PROPERTIES ENVIRONMENT "${CONNEXT_LIB_TEST_ENV}")
  endif()
endif()
