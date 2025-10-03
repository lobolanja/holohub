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

set(connext_rx_python_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/connext_op_rx)
set(connext_rx_python_dst_dir ${HOLOHUB_PYTHON_MODULE_OUT_DIR}/connext_ano/connext_op_rx)

add_custom_target(connext_op_rx_python ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${connext_rx_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${connext_rx_python_dst_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${connext_rx_python_src_dir} ${connext_rx_python_dst_dir}
    COMMENT "Copying Connext RX Python operator into holohub namespace"
)
