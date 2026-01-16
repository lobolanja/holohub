#!/bin/bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
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

# Setup environment for Advanced Network / DPDK
echo "Advanced Network Basic Demo - Development Container"
echo "===================================================="
echo ""
echo "DPDK and GPUDirect environment ready."
echo ""
echo "Build with: ./holohub build connext_ano_basic_app --build-type debug [--local]"
echo "Run TX:     ./holohub run connext_ano_basic_app --run-args='<config> tx' --docker-opts='-u root --privileged --network=host'"
echo "Run RX:     ./holohub run connext_ano_basic_app --run-args='<config> rx' --docker-opts='-u root --privileged --network=host'"
echo ""

exec /bin/bash
