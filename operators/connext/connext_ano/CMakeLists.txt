# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(connext_ano LANGUAGES NONE)

if(NOT HOLOHUB_BUILD_PYTHON)
  return()
endif()

if(NOT TARGET connext_lib_python)
  message(FATAL_ERROR "connext_lib_python target is required before building Connext ANO operators")
endif()

set(connext_ano_python_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/python/src/connext_ano)
set(connext_ano_python_tests_dir ${CMAKE_CURRENT_SOURCE_DIR}/python/tests)
set(connext_ano_pkg_dst ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_ano)

add_custom_target(connext_ano_python ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_ano_python_src_dir} ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_ano_python_tests_dir} ${connext_ano_pkg_dst}/tests
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/python/pyproject.toml ${connext_ano_pkg_dst}/pyproject.toml
  COMMENT "Staging holohub.connext_ano package"
)
add_dependencies(connext_ano_python connext_lib_python)

if(BUILD_TESTING)
  include(add_python_tests)
  add_python_tests(
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
    INPUT tests
    PYTEST_ARGS "-q"
  )
endif()
