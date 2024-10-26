#!/bin/bash

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Insurgency (222880)"
CMD="env LD_PRELOAD=/usr/lib32/libgcc_s.so.1 ${CMD}"
