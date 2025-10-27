# syntax=docker/dockerfile:1

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


############################################################
# Base image
############################################################

ARG BASE_IMAGE
ARG GPU_TYPE

FROM ${BASE_IMAGE} AS base

ARG DEBIAN_FRONTEND=noninteractive

# --------------------------------------------------------------------------
#
# Holohub dev setup
#

RUN mkdir -p /tmp/scripts
COPY holohub /tmp/scripts/
RUN mkdir -p /tmp/scripts/utilities
COPY utilities /tmp/scripts/utilities/
RUN chmod +x /tmp/scripts/holohub
RUN /tmp/scripts/holohub setup && rm -rf /var/lib/apt/lists/*

# Enable autocomplete in interactive shells
RUN echo ". /etc/bash_completion.d/holohub_autocomplete" >> /etc/bash.bashrc

# --------------------------------------------------------------------------
# CUDA runtime dependencies
# --------------------------------------------------------------------------

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        cuda-cudart-12-6 \
        cuda-nvtx-12-6 \
        libcublas-12-6 \
        libnpp-12-6 \
    && rm -rf /var/lib/apt/lists/*

ENV LD_LIBRARY_PATH=/usr/local/cuda/lib64:${LD_LIBRARY_PATH}

# --------------------------------------------------------------------------
# RTI Connext Python bindings
# --------------------------------------------------------------------------

RUN python3 -m pip install --no-cache-dir \
        rti.connext==7.3.0 \
        pytest

# Default path; override or mount your own license at runtime.
ENV RTI_LICENSE_FILE=/workspace/holohub/rti_license.dat

# Ensure Holohub datasets resolve inside the container
ENV HOLOSCAN_INPUT_PATH=/workspace/holohub/data

WORKDIR /workspace/holohub

ENV PYTHONPATH=/workspace/holohub/operators/connext/connext_lib/python/src:${PYTHONPATH}
ENV PYTHONPATH=/workspace/holohub/operators/connext/connext_ano/python/src:${PYTHONPATH}
ENV RTI_LICENSE_FILE=/workspace/holohub/rti_license.dat

CMD ["/bin/bash"]
