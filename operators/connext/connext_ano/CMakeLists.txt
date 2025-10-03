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

cmake_minimum_required(VERSION 3.20)
project(connext_ano LANGUAGES NONE)

# Register the C++/Python hybrid operators with the Holohub helper macro.
add_holohub_operator(connext_op_tx)
add_holohub_operator(connext_op_rx)

# Optionally expose the Python convenience package that re-exports the operators.
if(HOLOHUB_BUILD_PYTHON)
  add_subdirectory(python)
endif()

# Guarantee the shared connext_lib package is copied before any operator Python modules.
if(TARGET connext_lib_python)
  foreach(target IN ITEMS connext_op_tx_python connext_op_rx_python connext_ano_python)
    if(TARGET ${target})
      add_dependencies(${target} connext_lib_python)
    endif()
  endforeach()
endif()
