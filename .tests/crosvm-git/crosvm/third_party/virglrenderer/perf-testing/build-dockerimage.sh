#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
cd "${0%/*}"

export USER_ID=$(id -u)
export GROUP_ID=$(id -g)
src_root="$(realpath ..)"

docker build -t mesa \
    -f Docker/Dockerfile \
    --build-arg USER_ID=${USER_ID} \
    --build-arg GROUP_ID=${GROUP_ID} \
    "$@" \
    "${src_root}"
