# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/modules")

add_holohub_application(connext_app_cpp DEPENDS
                        OPERATORS connext_ops advanced_network)

add_holohub_application(connext_ano_basic_app DEPENDS
                        OPERATORS advanced_network connext_ano_lib)

