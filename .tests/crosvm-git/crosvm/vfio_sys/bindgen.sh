#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate vfio_sys bindgen bindings.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

source tools/impl/bindgen-common.sh

# VFIO_TYPE is translated as a u8 since it is a char constant, but it needs to be u32 for use in
# ioctl macros.
fix_vfio_type() {
    sed -E -e 's/^pub const VFIO_TYPE: u8 = (.*)u8;/pub const VFIO_TYPE: u32 = \1;/'
}

VFIO_EXTRA="// Added by vfio_sys/bindgen.sh
use zerocopy::AsBytes;
use zerocopy::FromBytes;
use zerocopy::FromZeroes;

// TODO(b/292077398): Upstream or remove ACPI notification forwarding support
pub const VFIO_PCI_ACPI_NTFY_IRQ_INDEX: std::os::raw::c_uint = 5;

#[repr(C)]
#[derive(Debug, Default)]
pub struct vfio_acpi_dsm {
    pub argsz: u32,
    pub padding: u32,
    pub args: __IncompleteArrayField<u8>,
}

#[repr(C)]
#[derive(Debug, Default, Copy, Clone)]
pub struct vfio_acpi_notify_eventfd {
    pub notify_eventfd: i32,
    pub reserved: u32,
}

#[repr(C)]
#[derive(Debug, Default)]
pub struct vfio_region_info_with_cap {
    pub region_info: vfio_region_info,
    pub cap_info: __IncompleteArrayField<u8>,
}

// vfio_iommu_type1_info_cap_iova_range minus the incomplete iova_ranges
// array, so that Copy/AsBytes/FromBytes can be implemented.
#[repr(C)]
#[derive(Debug, Default, Copy, Clone, AsBytes, FromZeroes, FromBytes)]
pub struct vfio_iommu_type1_info_cap_iova_range_header {
    pub header: vfio_info_cap_header,
    pub nr_iovas: u32,
    pub reserved: u32,
}

// Experimental Android uABI
pub const VFIO_PKVM_PVIOMMU: u32 = 11;"

bindgen_generate \
    --raw-line "${VFIO_EXTRA}" \
    --allowlist-var='VFIO_.*' \
    --blocklist-item='VFIO_DEVICE_API_.*_STRING' \
    --allowlist-type='vfio_.*' \
    --with-derive-custom "vfio_info_cap_header=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "vfio_iova_range=FromZeroes,FromBytes,AsBytes" \
    "${BINDGEN_LINUX}/include/uapi/linux/vfio.h" \
    -- \
    -D__user= \
    | replace_linux_int_types | fix_vfio_type \
    > vfio_sys/src/vfio.rs
