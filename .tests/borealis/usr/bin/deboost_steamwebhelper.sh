#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will be used to deboost the steamwebhelper process. It will scan
# repeatedly for this using pidof. Once found, all returned PID's will be
# reniced to deboost it as appropriate. All child threads will also be
# reniced.

EXECUTABLE_NAME=steamwebhelper
WAIT_TIMEOUT=20
POST_PIDOF_DELAY=5
NICE_VALUE=10

renice_pids() {
  for pid in $1; do
    renice -n "${NICE_VALUE}" -p "${pid}" &> /dev/null
    for task_pid in /proc/"${pid}"/task/*;do
      renice -n "${NICE_VALUE}" -p "$(basename "${task_pid}")" &> /dev/null
    done
  done
}

WAITCOUNT=${WAIT_TIMEOUT}
pidof "${EXECUTABLE_NAME}"

while [[ $? -eq 1 ]]; do
  sleep 1
  ((WAITCOUNT--))
  if [[ ${WAITCOUNT} -eq 0 ]]; then
    echo "Timed out waiting for ${EXECUTABLE_NAME}"
    exit
  fi
  pidof "${EXECUTABLE_NAME}"
done

sleep "${POST_PIDOF_DELAY}"
PIDLIST=$(pidof "${EXECUTABLE_NAME}")

renice_pids "${PIDLIST}"
