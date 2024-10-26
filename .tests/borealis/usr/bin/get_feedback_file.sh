#!/bin/bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

FILEPATH="${1}"
ALLOWED_PATHS=("/tmp/engine.log" "/tmp/steam-log.txt" "/tmp/launch-log.txt"
               "/tmp/fossilize-wrap-log.txt" "/tmp/proton_crashreports/")

function print_redacted {
  local filepath="${1}"

  # For user privacy, we should not capture user-generated names in
  # feedback reports. This pattern should catch Windows-style paths
  # as well as Linux-style paths.
  #
  # original: /mnt/external/0/MyFavoriteLibrary/steamapps/
  # redacted: /[REDACTED]/steamapps
  sed -re 's:(\\|/).*(\\|/)steamapps:/[REDACTED]/steamapps:g' "${filepath}"
}

if ! [[ " ${ALLOWED_PATHS[*]} " == *" ${FILEPATH} "* ]]; then
  exit 1
fi

if [[ -d "${FILEPATH}" ]]; then
  # Get only the most recent file
  FILEPATH="$(find "${FILEPATH}" -maxdepth 1 -type f -printf '%T@ %p\n' \
              | sort -nr | awk '(NR==1){print $2}')"
fi

if [[ -e "${FILEPATH}" ]]; then
  print_redacted "${FILEPATH}"
fi
