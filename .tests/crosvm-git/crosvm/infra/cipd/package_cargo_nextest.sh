#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

BUILD_DIR=$(mktemp -d)
cd "$BUILD_DIR" || exit 1

CIPD_ARGS=(
    -pkg-var "description:cargo-nextest modern test runner for cargo"
    -install-mode copy
    -ref latest
)


cd $(mktemp -d)
curl -L "https://get.nexte.st/latest/windows" | jar xv
cipd create -in "." -name "crosvm/cargo-nextest/windows-amd64" "${CIPD_ARGS[@]}"

cd $(mktemp -d)
curl -L "https://get.nexte.st/latest/linux" | tar zxv
cipd create -in "." -name "crosvm/cargo-nextest/linux-amd64" "${CIPD_ARGS[@]}"

