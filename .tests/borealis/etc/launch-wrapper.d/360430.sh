#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Mafia III: Definitive Edition (360430)"

if runtime_is_proton; then
  CMD="$(sed "s/2KLauncher\/LauncherPatcher.exe'.*/Mafia3DefinitiveEdition.exe'/" <<< "${CMD}")"
fi
