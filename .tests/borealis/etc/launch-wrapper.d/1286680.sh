#!/bin/bash

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Tiny Tina's Wonderlands (1286680)"

# DX 11 performs much better for this title
add_cmd_suffix_if_not_present "-dx11" "-dx12"
