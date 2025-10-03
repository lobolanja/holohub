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

# Guard: skip everything when python artifacts are not requested.
if(NOT HOLOHUB_BUILD_PYTHON)
  return()
endif()

# Paths for copying sources/tests into the holohub Python namespace in the build tree.
set(connext_lib_python_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src/connext_lib)
set(connext_lib_python_dst_dir ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_lib)

# Build rule: mirror sources, tests, and metadata every time the target is built.
add_custom_target(connext_lib_python ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${connext_lib_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_lib_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_lib_python_src_dir} ${connext_lib_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tests ${connext_lib_python_dst_dir}/tests
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${connext_lib_python_dst_dir}/README.md
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml ${connext_lib_python_dst_dir}/pyproject.toml
    COMMENT "Copying Connext Python library into holohub namespace"
)

# Optional test integration: only execute when the super-build asked for tests.
if(BUILD_TESTING)
  include(add_python_tests)

  if(NOT Python3_EXECUTABLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
  endif()

  # Probe: check that pytest is importable before we attempt collection.
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pytest --version
    RESULT_VARIABLE _pytest_check_result
    OUTPUT_QUIET
    ERROR_QUIET
  )

  if(_pytest_check_result EQUAL 0)
    # Force concise pytest discovery output while preserving any caller settings.
    set(_prev_pytest_addopts "$ENV{PYTEST_ADDOPTS}")
    set(ENV{PYTEST_ADDOPTS} "-q")

    # Register each test with CTest so they can run alongside other holohub suites.
    add_python_tests(
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      INPUT tests
      PYTEST_ARGS "-q"
    )

    # Restore the user's PYTEST_ADDOPTS environment to avoid side effects.
    if(_prev_pytest_addopts)
      set(ENV{PYTEST_ADDOPTS} "${_prev_pytest_addopts}")
    else()
      unset(ENV{PYTEST_ADDOPTS})
    endif()
  else()
    # Visibility: explain why the Connext tests did not get registered.
    message(WARNING "pytest is not available; skipping Connext library Python tests integration. Install pytest to enable ctest support.")
  endif()
endif()
