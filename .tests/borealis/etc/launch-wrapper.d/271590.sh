#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Grand Theft Auto V (271590)"

GUEST_MEM="$(awk '/MemTotal/{ print $2; }' /proc/meminfo)"
log "Guest memory ${GUEST_MEM} kB"
if [ "${GUEST_MEM}" -lt 12000000 ]; then
    echo "Running on an 8GB or smaller device; using min settings"
    add_cmd_suffix_if_not_present "-safemode" "-nosafemode"
fi
