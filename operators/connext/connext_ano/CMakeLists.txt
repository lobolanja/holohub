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

set(connext_ano_pkg_dst ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_ano)

add_custom_target(connext_ano_python ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py ${connext_ano_pkg_dst}/__init__.py
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/common.py ${connext_ano_pkg_dst}/common.py
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/connext_op_tx.py ${connext_ano_pkg_dst}/connext_op_tx.py
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/connext_op_rx.py ${connext_ano_pkg_dst}/connext_op_rx.py
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml ${connext_ano_pkg_dst}/pyproject.toml
  COMMENT "Staging holohub.connext_ano package"
)
add_dependencies(connext_ano_python connext_lib_python)

if(BUILD_TESTING)
  include(add_python_tests)
  add_python_tests(
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    INPUT tests
    PYTEST_ARGS "-q" "--override-ini" "pythonpath=${HOLOHUB_PYTHON_MODULE_OUT_DIR}:${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()
