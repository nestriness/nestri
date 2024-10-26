#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Regenerate kvm_sys bindgen bindings.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

source tools/impl/bindgen-common.sh

KVM_EXTRAS="// Added by kvm_sys/bindgen.sh
use zerocopy::AsBytes;
use zerocopy::FromBytes;
use zerocopy::FromZeroes;

// TODO(b/316337317): Update if new memslot flag is accepted in upstream
pub const KVM_MEM_NON_COHERENT_DMA: u32 = 8;
pub const KVM_CAP_USER_CONFIGURE_NONCOHERENT_DMA: u32 = 236;

// TODO(qwandor): Update this once the pKVM patches are merged upstream with a stable capability ID.
pub const KVM_CAP_ARM_PROTECTED_VM: u32 = 0xffbadab1;
pub const KVM_CAP_ARM_PROTECTED_VM_FLAGS_SET_FW_IPA: u32 = 0;
pub const KVM_CAP_ARM_PROTECTED_VM_FLAGS_INFO: u32 = 1;
pub const KVM_VM_TYPE_ARM_PROTECTED: u32 = 0x80000000;
pub const KVM_DEV_VFIO_PVIOMMU: u32 = 2;
pub const KVM_DEV_VFIO_PVIOMMU_ATTACH: u32 = 1;
#[repr(C)]
#[derive(Debug, Default, Copy, Clone)]
pub struct kvm_vfio_iommu_info {
    pub device_fd: i32,
    pub nr_sids: u32,
}
pub const KVM_DEV_VFIO_PVIOMMU_GET_INFO: u32 = 2;
#[repr(C)]
#[derive(Debug, Default, Copy, Clone)]
pub struct kvm_vfio_iommu_config {
    pub device_fd: i32,
    pub sid_idx: u32,
    pub vsid: u32,
}"

X86_64_EXTRAS="
// This is how zerocopy's author deal with bindings for __BindgenBitfieldUnit<Storage>, see:
// https://fuchsia-review.googlesource.com/c/859278/8/src/starnix/lib/linux_uapi/generate.py
unsafe impl<Storage> AsBytes for __BindgenBitfieldUnit<Storage>
where
    Storage: AsBytes,
{
    fn only_derive_is_allowed_to_implement_this_trait() {}
}

unsafe impl<Storage> FromBytes for __BindgenBitfieldUnit<Storage>
where
    Storage: FromBytes,
{
    fn only_derive_is_allowed_to_implement_this_trait() {}
}

unsafe impl<Storage> FromZeroes for __BindgenBitfieldUnit<Storage>
where
    Storage: FromZeroes,
{
    fn only_derive_is_allowed_to_implement_this_trait() {}
}"

bindgen_generate \
    --raw-line "${KVM_EXTRAS}" \
    --raw-line "${X86_64_EXTRAS}" \
    --blocklist-item='__kernel.*' \
    --blocklist-item='__BITS_PER_LONG' \
    --blocklist-item='__FD_SETSIZE' \
    --blocklist-item='_?IOC.*' \
    --with-derive-custom "kvm_regs=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_sregs=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_fpu=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_debugregs=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_xcr=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_xcrs=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_lapic_state=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_mp_state=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events__bindgen_ty_1=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events__bindgen_ty_2=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events__bindgen_ty_3=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events__bindgen_ty_4=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events__bindgen_ty_5=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_dtable=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_segment=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_pic_state=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_ioapic_state=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_pit_state2=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_clock_data=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_ioapic_state__bindgen_ty_1=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_ioapic_state__bindgen_ty_1__bindgen_ty_1=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_pit_channel_state=FromZeroes,FromBytes,AsBytes" \
    "${BINDGEN_LINUX_X86_HEADERS}/include/linux/kvm.h" \
    -- \
    -isystem "${BINDGEN_LINUX_X86_HEADERS}/include" \
    | replace_linux_int_types \
    > kvm_sys/src/x86/bindings.rs

bindgen_generate \
    --raw-line "${KVM_EXTRAS}" \
    --blocklist-item='__kernel.*' \
    --blocklist-item='__BITS_PER_LONG' \
    --blocklist-item='__FD_SETSIZE' \
    --blocklist-item='_?IOC.*' \
    --with-derive-custom "kvm_regs=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_sregs=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_fpu=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_vcpu_events__bindgen_ty_1=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "kvm_mp_state=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "user_fpsimd_state=FromZeroes,FromBytes,AsBytes" \
    --with-derive-custom "user_pt_regs=FromZeroes,FromBytes,AsBytes" \
    "${BINDGEN_LINUX_ARM64_HEADERS}/include/linux/kvm.h" \
    -- \
    -isystem "${BINDGEN_LINUX_ARM64_HEADERS}/include" \
    | replace_linux_int_types \
    > kvm_sys/src/aarch64/bindings.rs

bindgen_generate \
    --raw-line "${KVM_EXTRAS}" \
    --blocklist-item='__kernel.*' \
    --blocklist-item='__BITS_PER_LONG' \
    --blocklist-item='__FD_SETSIZE' \
    --blocklist-item='_?IOC.*' \
    "${BINDGEN_LINUX_RISCV_HEADERS}/include/linux/kvm.h" \
    -- \
    -isystem "${BINDGEN_LINUX_RISCV_HEADERS}/include" \
    | replace_linux_int_types \
    > kvm_sys/src/riscv64/bindings.rs
