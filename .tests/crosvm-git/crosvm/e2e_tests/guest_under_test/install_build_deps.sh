#!/bin/bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Installs additional dependencies needed to build guest_under_test on debian.

set -e

sudo apt install \
    gcc-aarch64-linux-gnu \
    qemu-user-static \
    squashfs-tools-ng \
    libelf-dev \
    flex \
    bison \
    bc \
    dwarves \
    binfmt-support
