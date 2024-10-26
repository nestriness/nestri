#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for XCOM 2 (268500)"

if runtime_is_proton; then
  # Check if War of the Chosen expansion is installed.
  # STEAM_COMPAT_INSTALL_PATH referenced but not assigned
  # shellcheck disable=SC2154
  if [ -f "${STEAM_COMPAT_INSTALL_PATH}/XCom2-WarOfTheChosen/Binaries/Win64/XCom2.exe" ]; then
    CMD="$(sed "s/2KLauncher\/LauncherPatcher.exe'.*/XCom2-WarOfTheChosen\/Binaries\/Win64\/XCom2.exe' -noRedScreens -review/" <<< "${CMD}")"
  else
    CMD="$(sed "s/2KLauncher\/LauncherPatcher.exe'.*/Binaries\/Win64\/XCom2.exe' -noRedScreens -review/" <<< "${CMD}")"
  fi
fi
