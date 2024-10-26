#!/bin/bash
# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Exports env variables to make the e2e_tests use a locally built
# kernel / rootfs.
#
# Note: `source` this file, do not run it if you want it to set the environmens
# variables for you.

ARCH=$(arch)
TARGET_DIR=$(cargo metadata --no-deps --format-version 1 |
    jq -r ".target_directory")
TARGET_DIR=${TARGET_DIR}/guest_under_test/${ARCH}

KERNEL_IMAGE=${TARGET_DIR}/bzImage
if [ -f ${KERNEL_IMAGE} ] ; then
    echo "Kernel image overrided: ${KERNEL_IMAGE}"
    export CROSVM_CARGO_TEST_KERNEL_IMAGE="${KERNEL_IMAGE}"
fi

ROOTFS_IMAGE=${TARGET_DIR}/rootfs
if [ -f ${ROOTFS_IMAGE} ] ; then
    echo "Rootfs image overrided: ${ROOTFS_IMAGE}"
    export CROSVM_CARGO_TEST_ROOTFS_IMAGE="${ROOTFS_IMAGE}"
fi

INITRD_IMAGE=${TARGET_DIR}/initramfs.cpio.gz
if [ -f ${INITRD_IMAGE} ] ; then
    echo "Initrd image overrided: ${INITRD_IMAGE}"
    export CROSVM_CARGO_TEST_INITRD_IMAGE="${INITRD_IMAGE}"
fi
