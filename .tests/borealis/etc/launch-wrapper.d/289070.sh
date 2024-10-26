#!/bin/bash

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Sid Meier's Civilization VI (289070)"

if runtime_is_proton; then
  CMD="$(sed "s/2KLauncher\/LauncherPatcher.exe'/Base\/Binaries\/Win64Steam\/CivilizationVI.exe'/" <<< "${CMD}")"
elif ! runtime_is_slr; then
  CMD="env LD_PRELOAD=/usr/lib/libfreetype.so.6 ${CMD}"
fi
