# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(connext_ano LANGUAGES NONE)

if(HOLOHUB_BUILD_PYTHON)
  add_subdirectory(python)
endif()
