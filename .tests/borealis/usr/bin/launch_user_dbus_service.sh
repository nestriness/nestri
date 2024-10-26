#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
exec 2> >(logger --tag=launch-user-dbus-service --id=$$)

# shellcheck disable=SC1091
source /tmp/.chronos_dbus_session_env.sh

export DISPLAY=:0

exec runuser -u "chronos" -- "$@"
