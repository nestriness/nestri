#!/bin/sh
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

filename="$(/usr/bin/generate-frame-log.sh)"
if [ -n "${filename}" ]; then
  /usr/bin/get_frame_summary.py "${filename}"
  cat "${filename}"
else
  echo "ERROR: unable to get frame log"
fi
