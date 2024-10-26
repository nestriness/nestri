#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

log() {
  logger --stderr --tag "steam-wrapper/link_shadercache_on_login" "$@"
}

# Steam apps path is created on first login
shadercache_path="$1"
mounted_shadercache_path="$2"
steam_apps_path="$3"

# During early stage of Steam installation, shadercache path is created.
log "Steam apps path not found, waiting for first login.."
while [[ ! -d "${steam_apps_path}" ]]; do
  # Race condition will happen if user manages to start installing something
  # within 0.5 seconds after logging in, but that is borderline impossible.
  sleep 0.5
done
log "Steam apps path found, creating shadercache directory"

if ! ln -s "${mounted_shadercache_path}" "${shadercache_path}"; then
  log "Failed to create symlink"
  exit 1
fi

exit 0
