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

# Guard: skip if the top-level build disabled Python packaging.
if(NOT HOLOHUB_BUILD_PYTHON)
  return()
endif()

# Paths that describe where the TX package lives and where it should be copied in the build tree.
set(connext_tx_python_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/connext_op_tx)
set(connext_tx_python_dst_dir ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_ano/connext_op_tx)

# Copy rule to stage the operator for holohub importers.
add_custom_target(connext_op_tx_python ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${connext_tx_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_tx_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_tx_python_src_dir} ${connext_tx_python_dst_dir}
    COMMENT "Copying Connext TX Python operator into holohub namespace"
)
