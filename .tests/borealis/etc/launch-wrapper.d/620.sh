#!/bin/bash

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Portal 2 (620)"

# Run under Vulkan unless OpenGL is explicitly requested.
add_cmd_suffix_if_not_present "-vulkan" "-gl"
