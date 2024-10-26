#!/usr/bin/env bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate gunyah_sys bindgen bindings.

set -euo pipefail
# assume in hypervisor/src/gunyah/gunyah_sys
cd "$(dirname "${BASH_SOURCE[0]}")/../../../.."

source tools/impl/bindgen-common.sh

# use local copy until gunyah.h is available in chromium monorepo
bindgen_generate \
    --blocklist-item='__kernel.*' \
    --blocklist-item='__BITS_PER_LONG' \
    --blocklist-item='__FD_SETSIZE' \
    --blocklist-item='_?IOC.*' \
    "hypervisor/src/gunyah/gunyah_sys/gunyah.h" \
    -- \
    -isystem "${BINDGEN_LINUX_ARM64_HEADERS}/include" \
    | replace_linux_int_types \
    > hypervisor/src/gunyah/gunyah_sys/bindings.rs
