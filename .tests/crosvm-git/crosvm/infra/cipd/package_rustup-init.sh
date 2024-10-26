#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

BUILD_DIR=$(mktemp -d)
cd "$BUILD_DIR" || exit 1

CIPD_ARGS=(
    -pkg-var "description:rustup-init to set up rustup.rs"
    -install-mode copy
    -ref latest
)

RUSTUP_WIN="https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe"
RUSTUP_LINUX="https://static.rust-lang.org/rustup/dist/x86_64-unknown-linux-gnu/rustup-init"

cd $(mktemp -d)
wget "$RUSTUP_WIN" -O "rustup-init.exe"
cipd create -in "." -name "crosvm/rustup-init/windows-amd64" "${CIPD_ARGS[@]}"

cd $(mktemp -d)
wget "$RUSTUP_LINUX" -O "rustup-init"
chmod +x "rustup-init"
cipd create -in "." -name "crosvm/rustup-init/linux-amd64" "${CIPD_ARGS[@]}"

