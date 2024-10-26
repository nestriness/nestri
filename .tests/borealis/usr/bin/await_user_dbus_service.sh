#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu
exec 2> >(logger --tag=await_user_dbus_service --id=$$)

# shellcheck disable=SC1091
source /tmp/.chronos_dbus_session_env.sh

export DISPLAY=:0

until exec runuser -u "chronos" -- gdbus wait --session "$1"; do
  sleep 0.5;
done
