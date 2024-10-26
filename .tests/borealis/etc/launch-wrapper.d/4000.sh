#!/bin/bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

log "Applying override for Garry's Mod (4000)"

# Override compat tool as Proton if the user has not specified any.
experimental_override_compat_tool proton-stable
