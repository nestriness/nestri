#!/bin/bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for The Sims 3 (47890)"

if runtime_is_proton; then
  CMD="$(sed "s/Sims3Launcher.exe'.*/TS3W.exe'/" <<< "${CMD}")"
fi
