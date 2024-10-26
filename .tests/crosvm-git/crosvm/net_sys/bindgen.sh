#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate net_sys bindgen bindings.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

source tools/impl/bindgen-common.sh

# Replace definition of struct sockaddr with an import of libc::sockaddr.
replace_sockaddr_libc() {
    # Match the following structure definition across multiple lines:
    #
    # #[repr(C)]
    # #[derive(Debug, Default, Copy, Clone)]
    # pub struct sockaddr {
    #     pub sa_family: sa_family_t,
    #     pub sa_data: [::std::os::raw::c_char; 14usize],
    # }
    sed -E \
        -e '1h;2,$H;$!d;g' \
        -e 's/#\[repr\(C\)\]\n#\[derive\([^)]+\)\]\npub struct sockaddr \{\n(    pub [^\n]+\n)+\}/\nuse libc::sockaddr;\n/'
}

# Remove custom sa_family_t type in favor of libc::sa_family_t.
remove_sa_family_t() {
    grep -v "pub type sa_family_t = "
}

bindgen_generate \
    --allowlist-type='ifreq' \
    --allowlist-type='net_device_flags' \
    --bitfield-enum='net_device_flags' \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/if.h" \
    | replace_linux_int_types \
    | replace_sockaddr_libc \
    | remove_sa_family_t \
    | rustfmt \
    > net_sys/src/iff.rs

bindgen_generate \
    --allowlist-type='sock_fprog' \
    --allowlist-var='TUN_.*' \
    --allowlist-var='IFF_NO_PI' \
    --allowlist-var='IFF_MULTI_QUEUE' \
    --allowlist-var='IFF_TAP' \
    --allowlist-var='IFF_VNET_HDR' \
    "${BINDGEN_LINUX}/include/uapi/linux/if_tun.h" \
    | replace_linux_int_types \
    > net_sys/src/if_tun.rs

bindgen_generate \
    --allowlist-var='SIOC.*' \
    "${BINDGEN_LINUX}/include/uapi/linux/sockios.h" \
    | replace_linux_int_types \
    > net_sys/src/sockios.rs
