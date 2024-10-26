#!/bin/bash
# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds and uploads prebuilts to cloud storage.
#
# Note: Only Googlers with access to the crosvm-testing cloud storage bin can
#       upload prebuilts.
#
# See README.md for how to uprev the prebuilt version.

set -e
cd "${0%/*}"

readonly PREBUILT_VERSION="$(cat ./PREBUILT_VERSION)"
readonly GS_PREFIX="gs://crosvm/integration_tests/guest"

function prebuilts_exist_error() {
    echo "Prebuilts of version ${PREBUILT_VERSION} already exist. See README.md"
    exit 1
}

function upload() {
    local arch=$1
    local remote_bzimage="${GS_PREFIX}-bzimage-${arch}-${PREBUILT_VERSION}"
    local remote_rootfs="${GS_PREFIX}-rootfs-${arch}-${PREBUILT_VERSION}"

    # Local files
    local cargo_target=$(cargo metadata --no-deps --format-version 1 |
        jq -r ".target_directory")
    local local_bzimage=${cargo_target}/guest_under_test/${arch}/bzImage
    local local_rootfs=${cargo_target}/guest_under_test/${arch}/rootfs

    echo "Checking if prebuilts already exist."
    gsutil stat "${remote_bzimage}" && prebuilts_exist_error
    gsutil stat "${remote_rootfs}" && prebuilts_exist_error

    echo "Building rootfs and kernel."
    make ARCH=${arch} "${local_bzimage}" "${local_rootfs}"

    echo "Uploading files."
    gsutil cp -n "${local_bzimage}" "${remote_bzimage}"
    gsutil cp -n "${local_rootfs}" "${remote_rootfs}"
}

upload x86_64
upload aarch64
