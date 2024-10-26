#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is to be run on the KVM host, outside the container

set -ex

# grab the pwd before changing it to this script's directory
pwd="${PWD}"

cd "${0%/*}"

exec docker run -it --rm \
    --privileged \
    --ipc=host \
    -v /dev/log:/dev/log \
    -v /dev/vhost-net:/dev/vhost-net \
    -v /sys/kernel/debug:/sys/kernel/debug \
    --volume "$pwd":/wd \
    --workdir /wd \
    -p 127.0.0.1:10000:10000/tcp \
    --entrypoint /usr/local/run_perfetto_ui.sh \
    mesa
