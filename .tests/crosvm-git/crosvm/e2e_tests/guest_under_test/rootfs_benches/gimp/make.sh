#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate compressed rootfs image for gimp e2e benchmark
# Result will be stored as /tmp/crosvm_e2e_test_guest_gimp.img.zst

cd "$(dirname "$0")"
podman build -t crosvm_e2e_test_guest_gimp ../../../../ -f e2e_tests/guest_under_test/rootfs_benches/gimp/ContainerFile
CONTAINER=$(podman create crosvm_e2e_test_guest_gimp)
podman export $CONTAINER > /tmp/crosvm_e2e_test_guest_gimp.tar
podman rm $CONTAINER
virt-make-fs --format=raw --size=+100M --type=ext4 /tmp/crosvm_e2e_test_guest_gimp.tar /tmp/crosvm_e2e_test_guest_gimp.img
rm /tmp/crosvm_e2e_test_guest_gimp.tar
zstd --rm /tmp/crosvm_e2e_test_guest_gimp.img
