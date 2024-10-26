#!/bin/sh

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Compile device tree source files to binaries used as test inputs.
# Run this when source files are changed.

# Check if dtc is present
if ! command -v dtc >/dev/null 2>&1; then
    echo "Error: device tree compiler (dtc) not found."
    exit 1
fi

# Compile all dts files
testfiles_loc="$(dirname -- "$0")"
for source_file in "$testfiles_loc"/*.dts; do
    if [ -f "$source_file" ]; then
        binary_file="${source_file%.dts}.dtb"
        dtc -@ -I dts -O dtb -o "$binary_file" "$source_file"
    fi
done
