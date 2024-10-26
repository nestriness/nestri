#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Sid Meier's Pirates! (3920)"

# b/242030275: Works around rendering artifacts in cutscenes.
export PROTON_USE_WINED3D=1
