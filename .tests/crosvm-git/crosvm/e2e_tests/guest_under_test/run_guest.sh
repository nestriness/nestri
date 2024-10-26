#!/bin/bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Example invocation to run the locally built guest for debugging

CARGO_TARGET=$(cargo metadata --no-deps --format-version 1 |
        jq -r ".target_directory")
GUEST_DIR="${CARGO_TARGET}/guest_under_test/x86_64"

cargo run -p crosvm -- \
    run \
    -p "init=/bin/sh" \
    --root "${GUEST_DIR}/rootfs"\
    "${GUEST_DIR}/bzImage" \
    "$@"
