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

FROM holohub:adv_networking_bench AS base


# Install connext 7.3.0 from .deb package
# Download the public key for the RTI APT repository
RUN curl -sSL -o /usr/share/keyrings/rti-official-archive.gpg \
https://packages.rti.com/deb/official/repo.key

# Add the RTI APT repository to the sources list
RUN printf -- "deb [arch=%s signed-by=%s] %s %s main\n" \
    $(dpkg --print-architecture) \
    /usr/share/keyrings/rti-official-archive.gpg \
    https://packages.rti.com/deb/official \
    $(. /etc/os-release && echo ${VERSION_CODENAME}) \
    | tee /etc/apt/sources.list.d/rti.list >/dev/null

# Set environment variable to accept the RTI License Agreement
RUN export RTI_LICENSE_AGREEMENT_ACCEPTED=accepted && apt update \
    && apt install -y rti-connext-dds-7.3.0

# For RTI Connext DDS
RUN apt update \
    && apt install --no-install-recommends -y \
    gdb \
    vim \
    openjdk-21-jre
RUN echo 'export JREHOME=$(readlink /etc/alternatives/java | sed -e "s/\/bin\/java//")' >> /etc/bash.bashrc

# Set default Holohub data directory
ENV HOLOSCAN_INPUT_PATH=/workspace/holohub/data

ENV NDDSHOME=/opt/rti.com/rti_connext_dds-7.3.0
ENV RTI_CONNEXT_DDS_DIR=$NDDSHOME

COPY ./applications/connext/connext_ano_basic_app/bootstrap.sh /bootstrap.sh
RUN chmod +x /bootstrap.sh

CMD ["/bootstrap.sh"]
