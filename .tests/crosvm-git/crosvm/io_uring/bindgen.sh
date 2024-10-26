#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate io_uring bindgen bindings.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

source tools/impl/bindgen-common.sh

bindgen_generate \
    --allowlist-type='io_uring_.*' \
    --allowlist-var='IO_URING_.*' \
    --allowlist-var='IORING_.*' \
    "${BINDGEN_LINUX}/include/uapi/linux/io_uring.h" \
    | replace_linux_int_types | rustfmt \
    > io_uring/src/bindings.rs
