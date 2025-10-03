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

ARG BASE_IMAGE
ARG GPU_TYPE

############################################################
# Base image
############################################################

FROM ${BASE_IMAGE} AS base
SHELL ["/bin/bash", "-eou", "pipefail", "-c"]

ARG DEBIAN_FRONTEND=noninteractive
ARG CMAKE_BUILD_TYPE=Release

# Copy holohub helper script and utilities so we can bootstrap the dev env
RUN mkdir -p /tmp/scripts
COPY holohub /tmp/scripts/
RUN mkdir -p /tmp/scripts/utilities
COPY utilities /tmp/scripts/utilities/
RUN chmod +x /tmp/scripts/holohub
RUN /tmp/scripts/holohub setup && rm -rf /var/lib/apt/lists/*

# Enable holohub autocomplete for interactive shells
RUN echo ". /etc/bash_completion.d/holohub_autocomplete" >> /etc/bash.bashrc

# Baseline packages commonly required by Holohub operators
RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        libgstreamer1.0-0 \
        libgstreamer-plugins-base1.0-0 \
        libgles2 \
        libopengl0 \
        ninja-build \
        curl \
        ca-certificates \
        python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir meson pytest rti.connext==7.3.0

# --------------------------------------------------------------------------
# Install RTI Connext DDS 7.3.0
# --------------------------------------------------------------------------
RUN curl -sSL -o /usr/share/keyrings/rti-official-archive.gpg \
    https://packages.rti.com/deb/official/repo.key

RUN printf -- "deb [arch=%s signed-by=%s] %s %s main\n" \
    $(dpkg --print-architecture) \
    /usr/share/keyrings/rti-official-archive.gpg \
    https://packages.rti.com/deb/official \
    $(. /etc/os-release && echo ${VERSION_CODENAME}) \
    | tee /etc/apt/sources.list.d/rti.list >/dev/null

RUN export RTI_LICENSE_AGREEMENT_ACCEPTED=accepted \
    && apt-get update \
    && apt-get install -y --no-install-recommends rti-connext-dds-7.3.0 \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update \
    && apt-get install --no-install-recommends -y openjdk-21-jre \
    && rm -rf /var/lib/apt/lists/*
RUN echo 'export JREHOME=$(readlink /etc/alternatives/java | sed -e "s/\/bin\/java//")' >> /etc/bash.bashrc

ENV HOLOSCAN_INPUT_PATH=/workspace/holohub/data
ENV NDDSHOME=/opt/rti.com/rti_connext_dds-7.3.0
ENV RTI_CONNEXT_DDS_DIR=${NDDSHOME}

# Provide a bootstrap script placeholder (can be overridden per project)
COPY ./applications/dds/dds_video/bootstrap.sh /bootstrap.sh
RUN chmod +x /bootstrap.sh

CMD ["/bootstrap.sh"]
