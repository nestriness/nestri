#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

BUILD_DIR=$(mktemp -d)
cd "$BUILD_DIR" || exit 1

CIPD_ARGS=(
    -pkg-var "description:codecov.io uploader"
    -install-mode copy
    -ref latest
)

CODECOV_URL="https://uploader.codecov.io/v0.2.5/linux/codecov"

wget -q "$CODECOV_URL" -O "codecov"
echo "66cbf87269acc529c87f6ea29395ba329f528e92cfda4fc199eab460123e18b6  codecov" | sha256sum --check --status
chmod +x codecov
cipd create -in "." -name "crosvm/codecov/linux-amd64" "${CIPD_ARGS[@]}"
