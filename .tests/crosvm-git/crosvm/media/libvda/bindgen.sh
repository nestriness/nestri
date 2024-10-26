#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate libvda bindgen bindings.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

source tools/impl/bindgen-common.sh

bindgen_generate \
    --allowlist-type='video_.*' \
    "${BINDGEN_PLATFORM2}/arc/vm/libvda/libvda_common.h" \
    > media/libvda/src/bindings.rs

bindgen_generate \
    --raw-line 'pub use crate::bindings::*;' \
    --allowlist-function 'initialize' \
    --allowlist-function 'deinitialize' \
    --allowlist-function 'get_vda_capabilities' \
    --allowlist-function 'init_decode_session' \
    --allowlist-function 'close_decode_session' \
    --allowlist-function 'vda_.*' \
    --allowlist-type 'vda_.*' \
    --blocklist-type 'video_.*' \
    "${BINDGEN_PLATFORM2}/arc/vm/libvda/libvda_decode.h" \
    -- \
    -I "${BINDGEN_PLATFORM2}" \
    > media/libvda/src/decode/bindings.rs

bindgen_generate \
    --raw-line 'pub use crate::bindings::*;' \
    --allowlist-function 'initialize_encode' \
    --allowlist-function 'deinitialize_encode' \
    --allowlist-function 'get_vea_capabilities' \
    --allowlist-function 'init_encode_session' \
    --allowlist-function 'close_encode_session' \
    --allowlist-function 'vea_.*' \
    --allowlist-type 'vea_.*' \
    --blocklist-type 'video_.*' \
    "${BINDGEN_PLATFORM2}/arc/vm/libvda/libvda_encode.h" \
    -- \
    -I "${BINDGEN_PLATFORM2}" \
    > media/libvda/src/encode/bindings.rs
