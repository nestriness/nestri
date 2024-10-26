#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

mount_root () {
    if [ -f "/dev/sda" ]; then
        mount /dev/sda /newroot
    else
        mount /dev/vda /newroot
    fi
}

mount -t proc /proc -t proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

mount_root

if mount_root; then
    mkdir -p /newroot/proc /newroot/sys /newroot/dev || true

    mount --move /sys /newroot/sys
    mount --move /proc /newroot/proc
    mount --move /dev /newroot/dev
    ln -sf /proc/self/fd /newroot/dev/fd

    cp /bin/delegate /newroot/bin/delegate || true

    cd /newroot && chroot /newroot /bin/delegate
else
    exec /bin/delegate
fi
