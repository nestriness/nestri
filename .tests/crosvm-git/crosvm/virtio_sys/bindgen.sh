#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate virtio_sys bindgen bindings.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

source tools/impl/bindgen-common.sh

bindgen_generate \
    --allowlist-type='vhost_.*' \
    --allowlist-var='VHOST_.*' \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/vhost.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    > virtio_sys/src/vhost.rs

VIRTIO_CONFIG_EXTRA="// Added by virtio_sys/bindgen.sh
pub const VIRTIO_CONFIG_S_SUSPEND: u32 = 16;
pub const VIRTIO_F_SUSPEND: u32 = 42;"

bindgen_generate \
    --raw-line "${VIRTIO_CONFIG_EXTRA}" \
    --allowlist-var='VIRTIO_.*' \
    --allowlist-type='virtio_.*' \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_config.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    > virtio_sys/src/virtio_config.rs

VIRTIO_FS_EXTRA="// Added by virtio_sys/bindgen.sh
use data_model::Le32;
use zerocopy::AsBytes;
use zerocopy::FromBytes;
use zerocopy::FromZeroes;"

bindgen_generate \
    --raw-line "${VIRTIO_FS_EXTRA}" \
    --allowlist-var='VIRTIO_FS_.*' \
    --allowlist-type='virtio_fs_.*' \
    --with-derive-custom "virtio_fs_config=FromZeroes,FromBytes,AsBytes" \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_fs.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    | replace_linux_endian_types \
    > virtio_sys/src/virtio_fs.rs

VIRTIO_IDS_EXTRAS="
//! This file defines virtio device IDs. IDs with large values (counting down
//! from 63) are nonstandard and not defined by the virtio specification.

// Added by virtio_sys/bindgen.sh - do not edit the generated file.
// TODO(b/236144983): Fix this id when an official virtio-id is assigned to this device.
pub const VIRTIO_ID_PVCLOCK: u32 = 61;
"

bindgen_generate \
    --raw-line "${VIRTIO_IDS_EXTRAS}" \
    --allowlist-var='VIRTIO_ID_.*' \
    --allowlist-type='virtio_.*' \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_ids.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    | rustfmt \
    > virtio_sys/src/virtio_ids.rs

VIRTIO_NET_EXTRA="// Added by virtio_sys/bindgen.sh
use zerocopy::AsBytes;
use zerocopy::FromBytes;
use zerocopy::FromZeroes;"

bindgen_generate \
    --raw-line "${VIRTIO_NET_EXTRA}" \
    --allowlist-var='VIRTIO_NET_.*' \
    --allowlist-type='virtio_net_.*' \
    --blocklist-type='virtio_net_ctrl_mac' \
    --with-derive-custom "virtio_net_hdr=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "virtio_net_hdr_mrg_rxbuf=FromZeroes,FromBytes,AsBytes" \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_net.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    > virtio_sys/src/virtio_net.rs

bindgen_generate \
    --allowlist-var='VRING_.*' \
    --allowlist-var='VIRTIO_RING_.*' \
    --allowlist-type='vring.*' \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_ring.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    > virtio_sys/src/virtio_ring.rs

bindgen_generate \
    --allowlist-var='VIRTIO_.*' \
    --allowlist-type='virtio_.*' \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_mmio.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    > virtio_sys/src/virtio_mmio.rs

VIRTIO_VSOCK_EXTRA="// Added by virtio_sys/bindgen.sh
use data_model::Le16;
use data_model::Le32;
use data_model::Le64;
use zerocopy::AsBytes;"

bindgen_generate \
    --raw-line "${VIRTIO_VSOCK_EXTRA}" \
    --allowlist-var='VIRTIO_VSOCK_.*' \
    --allowlist-type='virtio_vsock_.*' \
    --with-derive-custom "virtio_vsock_event=AsBytes" \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_vsock.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    | replace_linux_endian_types \
    > virtio_sys/src/virtio_vsock.rs

VIRTIO_SCSI_EXTRA="// Added by virtio_sys/bindgen.sh
use zerocopy::AsBytes;
use zerocopy::FromBytes;
use zerocopy::FromZeroes;"

bindgen_generate \
    --raw-line "${VIRTIO_SCSI_EXTRA}" \
    --allowlist-var='VIRTIO_SCSI_.*' \
    --allowlist-type='virtio_scsi_.*' \
    --blocklist-type='virtio_scsi_cmd_req_pi' \
    --with-derive-custom "virtio_scsi_config=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "virtio_scsi_cmd_req=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "virtio_scsi_cmd_resp=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "virtio_scsi_ctrl_tmf_req=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "virtio_scsi_ctrl_an_req=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "virtio_scsi_ctrl_tmf_resp=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "virtio_scsi_ctrl_an_resp=FromZeroes,FromBytes,AsBytes" \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/virtio_scsi.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    | replace_linux_endian_types \
    > virtio_sys/src/virtio_scsi.rs
