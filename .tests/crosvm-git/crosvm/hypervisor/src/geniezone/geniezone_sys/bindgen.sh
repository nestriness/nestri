#!/usr/bin/env bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate geniezone_sys bindgen bindings.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../../../.."

source tools/impl/bindgen-common.sh

GZVM_SYS_BASE="hypervisor/src/geniezone/geniezone_sys"
GZVM_BINDINGS="${GZVM_SYS_BASE}/aarch64/bindings.rs"

GZVM_HEADER_FILE="${BINDGEN_LINUX_ARM64_HEADERS}/include/linux/gzvm_common.h"

bindgen_generate \
    --blocklist-item='__kernel.*' \
    --blocklist-item='__BITS_PER_LONG' \
    --blocklist-item='__FD_SETSIZE' \
    --blocklist-item='_?IOC.*' \
    ${GZVM_HEADER_FILE} \
    -- \
    -isystem "${BINDGEN_LINUX_ARM64_HEADERS}/include" \
    | replace_linux_int_types \
    > ${GZVM_BINDINGS}
