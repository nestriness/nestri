#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate compressed rootfs image for postgres e2e benchmark
# Result will be stored as /tmp/psql.img.zst

CONTAINER=$(podman create ghcr.io/cloudnative-pg/postgresql:15.3)
podman export $CONTAINER > /tmp/psql.tar
podman rm $CONTAINER
virt-make-fs --format=raw --size=+512M --type=ext4 /tmp/psql.tar /tmp/psql.img
rm /tmp/psql.tar
zstd --rm /tmp/psql.img
