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

# Guard: only author the holohub Python package when Python artifacts are enabled.
if(NOT HOLOHUB_BUILD_PYTHON)
  return()
endif()

# Destination directory where the holohub namespace mirrors the ANO package.
set(connext_ano_pkg_dst_dir ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_ano)

# Copy rule: create the package folder and drop the aggregator __init__.py.
add_custom_target(connext_ano_python ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_ano_pkg_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/connext_ano/__init__.py ${connext_ano_pkg_dst_dir}/__init__.py
    COMMENT "Preparing holohub.connext_ano package entry point"
)

# Ensure the umbrella package waits for the individual operator wrappers to be staged.
if(TARGET connext_op_tx_python)
  add_dependencies(connext_ano_python connext_op_tx_python)
endif()
if(TARGET connext_op_rx_python)
  add_dependencies(connext_ano_python connext_op_rx_python)
endif()


# Optional smoke tests: wire the pytest suite into ctest when requested.
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(_connext_ano_pytest_path "${CMAKE_CURRENT_SOURCE_DIR}/connext_op_tx/python:${CMAKE_CURRENT_SOURCE_DIR}/connext_op_rx/python:${CMAKE_CURRENT_SOURCE_DIR}/connext_ano")
  add_test(NAME connext_ano_python_tests
    COMMAND ${Python3_EXECUTABLE} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/tests
  )
  set_tests_properties(connext_ano_python_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    # Inject the operator package paths so pytest can import the staged modules.
    ENVIRONMENT "PYTHONPATH=${_connext_ano_pytest_path}:$ENV{PYTHONPATH}"
  )
endif()
