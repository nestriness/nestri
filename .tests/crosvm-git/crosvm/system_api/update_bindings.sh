#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

STUB_DIR=$(dirname "$0")
SYSTEM_API_DIR="$HOME/chromiumos/src/platform2/system_api"

if ! [ -e "$SYSTEM_API_DIR" ]; then
    echo "This script must be run from a ChromeOS checkout and inside cros_sdk."
fi

# The system_api build.rs will generate bindings in $SYSTEM_API_DIR/src
(cd "$SYSTEM_API_DIR" && cargo build)

FILES=(
    "src/bindings/client/org_chromium_spaced.rs"
    "src/bindings/client/org_chromium_vtpm.rs"
    "src/protos/spaced.rs"
    "src/protos/vtpm_interface.rs"
)

for FILE in "${FILES[@]}"; do
    TARGET_DIR=$(dirname "$STUB_DIR/$FILE")
    mkdir -p "$TARGET_DIR"
    cp "$SYSTEM_API_DIR/$FILE" "$TARGET_DIR"
done
