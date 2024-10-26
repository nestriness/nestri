#!/bin/bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for 7 Days to Die (251570)"

# Override GL driver to use zink
export ZINK=1
