# syntax=docker/dockerfile:1.3-labs
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

FROM docker.io/debian:testing
ARG BOARD=hatch

RUN apt update \
    && apt install -y git python3-pip curl sudo wget procps

# Add a new password-less sudoer user crosvmdev
RUN useradd -ms /bin/bash crosvmdev \
    && usermod -aG sudo crosvmdev \
    && echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

COPY entrypoint.sh /tools/
RUN chmod 755 /tools/entrypoint.sh
ENTRYPOINT [ "/tools/entrypoint.sh" ]

# Following operations will be run as crosvmdev to ensure correct permission.
USER crosvmdev
WORKDIR /home/crosvmdev

RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

# Prepare path to depot tools for crosvmdev
RUN echo 'export PATH=/home/crosvmdev/depot_tools:$PATH' >> /home/crosvmdev/.profile
ENV PATH="/home/crosvmdev/depot_tools:${PATH}"

RUN mkdir /home/crosvmdev/chromiumos

WORKDIR /home/crosvmdev/chromiumos

RUN git config --global user.email crosvmdev@crosvm.dev && git config --global user.name crosvmdev
RUN repo init -u https://chromium.googlesource.com/chromiumos/manifest -g minilayout,crosvm -b stable --depth=1
RUN repo sync -j 8 --current-branch

RUN --security=insecure cros_sdk --create && rm /home/crosvmdev/chromiumos/.cache/sdks/*
RUN --security=insecure cros_sdk setup_board --board=${BOARD}
RUN --security=insecure cros_sdk emerge-${BOARD} --update --deep -j$(nproc) chromeos-base/crosvm
