#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run this script to re-generate the seccomp/*/constants.json files for
# each architecture.

set -ex
cd "$(dirname "${BASH_SOURCE[0]}")/.."

MINIJAIL_DIR=$(realpath "third_party/minijail")
SECCOMP_DIR=$(realpath seccomp)

export SRC="$MINIJAIL_DIR"

# Create temporary directory for build artifacts and make sure it's cleaned up.
TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# Create bindings for each platform
for arch in "x86_64" "arm" "aarch64" "riscv64"; do
    BUILD_DIR="$TMP_DIR/$arch"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Pick the right cross-compiler
    if [ "$arch" = "x86_64" ]; then
        export CC="gcc"
        TARGET="x86_64-unknown-linux-gnu"
    elif [ "$arch" = "arm" ]; then
        export CC="arm-linux-gnueabihf-gcc"
        TARGET="armv7-unknown-linux-gnueabihf"
    elif [ "$arch" = "aarch64" ]; then
        export CC="aarch64-linux-gnu-gcc"
        TARGET="aarch64-unknown-linux-gnu"
    elif [ "$arch" = "riscv64" ]; then
        export CC="riscv64-unknown-linux-gnu-gcc"
        TARGET="riscv64-unknown-linux-gnu"
    fi

    "$MINIJAIL_DIR/gen_constants.sh" "libconstants.gen.c"
    "$MINIJAIL_DIR/gen_syscalls.sh" "libsyscalls.gen.c"

    clang \
        -target "$TARGET" \
        -S \
        -emit-llvm \
        -I "$MINIJAIL_DIR" \
        "libconstants.gen.c" \
        "libsyscalls.gen.c"

    "$MINIJAIL_DIR/tools/generate_constants_json.py" \
        --output "$SECCOMP_DIR/$arch/constants.json" \
        "libconstants.gen.ll" \
        "libsyscalls.gen.ll"
done
