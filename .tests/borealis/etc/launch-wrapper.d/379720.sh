#!/bin/bash

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for DOOM (379720)"
# Run under Vulkan if OpenGL is not specifically requested.
add_cmd_suffix_if_not_present "+r_renderAPI 1" "\+r_renderAPI 0"
