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

if(NOT HOLOHUB_BUILD_PYTHON)
  return()
endif()

set(connext_lib_python_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src/connext_lib)
set(connext_lib_python_dst_dir ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_lib)

add_custom_target(connext_lib_python ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${connext_lib_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_lib_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_lib_python_src_dir} ${connext_lib_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/tests ${connext_lib_python_dst_dir}/tests
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${connext_lib_python_dst_dir}/README.md
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml ${connext_lib_python_dst_dir}/pyproject.toml
    COMMENT "Copying Connext Python library into holohub namespace"
)
