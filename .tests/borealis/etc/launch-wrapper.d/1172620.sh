#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is sourced from /usr/bin/launch-wrap.sh to apply this game-
# specific override.

log "Applying override for Sea of Thieves (1172620)"

# This game often has audio glitches when it uses ALSA device directly.
# Let this game use PulseAudio by removing the PULSE_SERVER variable,
# that is set to be a fake PulseAudio socket from /usr/bin/launch-wrap.sh
unset PULSE_SERVER
