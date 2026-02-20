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
echo "Connext Holoscan Operator Demo Application- Development Container"
echo "===================================================="
echo ""
echo "DPDK and GPUDirect environment ready."
echo ""
echo "Build with: ./holohub build connext_app_cpp --build-type debug --local --configure-args=\"-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0\""
echo "Run TX:     ./holohub run connext_app_cpp --run-args='tx applications/connext/connext_app_cpp/demo_tx.yaml' --local"
echo "Run RX:     ./holohub run connext_app_cpp --run-args='rx applications/connext/connext_app_cpp/demo_rx.yaml' --local"
echo ""

source /opt/rti.com/rti_connext_dds-7.3.0/resource/scripts/rtisetenv_armv8Linux4gcc7.3.0.bash 
exec /bin/bash
