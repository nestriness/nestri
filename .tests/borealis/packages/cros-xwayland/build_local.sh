#!/bin/bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if ! command -v arch-meson &>/dev/null; then
  echo "The script is intended to run in Borealis VM or Docker build container"
  exit 1
fi

if [[ -z "$2" ]]; then
  echo "Expected usage:"
  echo "  build_local.sh <path_to_xserver_repo> <path_to_export_binary>"
  exit 1
fi

set -e
set -x

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
START_DIR="$(pwd)"
SRC="${START_DIR}/xwayland-tmp"

# Create copy of the source code provided, so that we can patch over and build
# it without modifying original code.
cp -r "$1" "${SRC}"

source "${SCRIPT_DIR}/PKGBUILD"

# Patch and build using defined functions in PKGBUILD
srcdir="${SCRIPT_DIR}" pkgver="tmp" prepare
cd "${START_DIR}"
pkgver="tmp" build

cp build/hw/xwayland/Xwayland "$2"
