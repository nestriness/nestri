#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run provided command or interactive shell
if [[ $# -eq 0 ]]; then
    sudo -u crosvmdev /bin/bash -l
else
    sudo -u crosvmdev /bin/bash -l -c "$*"
fi
