#!/bin/bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Counter-Strike: Source (240)"

# Disable tiled modifiers for WSI images to workaround host driver sync
# issues on ANV.
# TODO(b/320764983): Remove once bug is fixed.
export VN_PERF=no_tiled_wsi_image
