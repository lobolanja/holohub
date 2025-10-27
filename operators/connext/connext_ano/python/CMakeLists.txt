# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Guard: skip everything when python artifacts are not requested.
if(NOT HOLOHUB_BUILD_PYTHON)
  return()
endif()

if(NOT TARGET connext_lib_python)
  message(FATAL_ERROR "connext_lib_python target is required before building Connext ANO operators")
endif()

set(connext_ano_python_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src/connext_ano)
set(connext_ano_python_tests_dir ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(connext_ano_pkg_dst ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_ano)

add_custom_target(connext_ano_python ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_ano_python_src_dir} ${connext_ano_pkg_dst}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_ano_python_tests_dir} ${connext_ano_pkg_dst}/tests
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${connext_ano_pkg_dst}/README.md
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml ${connext_ano_pkg_dst}/pyproject.toml
  COMMENT "Staging holohub.connext_ano package"
)
add_dependencies(connext_ano_python connext_lib_python)

if(BUILD_TESTING)
  include(add_python_tests)

  if(NOT Python3_EXECUTABLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
  endif()

  execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pytest --version
    RESULT_VARIABLE _pytest_check_result
    OUTPUT_QUIET
    ERROR_QUIET
  )

  if(_pytest_check_result EQUAL 0)
    set(_prev_pytest_addopts "$ENV{PYTEST_ADDOPTS}")
    set(ENV{PYTEST_ADDOPTS} "-q")

    add_python_tests(
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      INPUT tests
      PYTEST_ARGS "-q"
    )

    if(_prev_pytest_addopts)
      set(ENV{PYTEST_ADDOPTS} "${_prev_pytest_addopts}")
    else()
      unset(ENV{PYTEST_ADDOPTS})
    endif()
  else()
    message(WARNING "pytest is not available; skipping Connext ANO Python tests integration. Install pytest to enable ctest support.")
  endif()
endif()
