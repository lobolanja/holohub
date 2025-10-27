# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

if(NOT HOLOHUB_BUILD_PYTHON)
  return()
endif()

add_custom_target(connext_python_app ALL
  DEPENDS connext_lib_python connext_ano_python
)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
  DESTINATION examples/connext
  COMPONENT connext-python
  FILES_MATCHING PATTERN "*.py"
)

install(
  FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/metadata.json
    ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml
  DESTINATION examples/connext
  COMPONENT connext-python
)

if(BUILD_TESTING)
  include(add_python_tests)

  if(NOT Python3_EXECUTABLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
  endif()


  set(_prev_pytest_addopts "$ENV{PYTEST_ADDOPTS}")
  set(ENV{PYTEST_ADDOPTS} "-q")

  add_python_tests(
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    INPUT tests
    PYTEST_ARGS "-q"
  )
endif()
