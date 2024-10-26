#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for DOOM Eternal (782330)"

# This game currently requires disabling of async descriptor behavior in Venus
# TODO(b/209698935): Remove once bug is fixed.

# DOOM Eternal uses its own native Vulkan rendering engine that assumes a
# relaxed validation of descriptor set allocations vs. the requested
# descriptor pool contents. The spec permits drivers to fail allocations
# that take this relaxed approach, but many native drivers silently allow
# it. Venus contains an optimization that must strictly enforce the
# validation, but some games (e.g. DOOM Eternal) don't expect it.
export VN_PERF=no_async_set_alloc
