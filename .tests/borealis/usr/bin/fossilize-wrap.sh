#!/bin/bash

# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Wrapper for launching Fossilize replay for Steam games with additional
# tracing, logging and configuration. To configure, create
# /tmp/fossilize-options.sh and set the environment variables listed below.

# NOTE: that hitting "Skip" in the UI terminates this script entirely.
# Terminating this script (either through manual "Skip" or fossilize/download)
# completion causes any spawned background processes to terminate.

# NOTE: If fossilize process had finished successfully before (ie. replay_cache
# exists in the guest) this script does not run.

# Allow overriding /tmp in unit tests.
: "${FOSSILIZE_WRAP_TMP:=/tmp}"

# Read options that were placed in a launch options script.
: "${FOSSILIZE_OPTIONS:="${FOSSILIZE_WRAP_TMP}/fossilize-options.sh"}"
if [[ -f "${FOSSILIZE_OPTIONS}" ]]; then
  # ShellCheck can't follow non-constant source.
  # shellcheck disable=SC1090
  source "${FOSSILIZE_OPTIONS}"
  SOURCED_FOSSILIZE_OPTIONS="${FOSSILIZE_OPTIONS}"
else
  SOURCED_FOSSILIZE_OPTIONS=""
fi

# Configuration that can be updated via environment setting or modifications
# to the file $FOSSILIZE_OPTIONS.
: "${DOWNLOAD_SHADER_CACHE:="1"}"
: "${ENV_DUMP:="0"}"
: "${OUTPUT_DUMP:="0"}"

: "${OUTPUT_DIR:="${FOSSILIZE_WRAP_TMP}"}"

: "${FOSSILIZE_LOCAL_LOG:="${OUTPUT_DIR}/fossilize-local-log.txt"}"
: "${FOSSILIZE_WRAP_ENV:="${OUTPUT_DIR}/fossilize-wrap.env"}"
: "${FOSSILIZE_WRAP_LOG:="${OUTPUT_DIR}/fossilize-wrap-log.txt"}"

FOSSILIZE_ARGS="$*"

# Log to /tmp/fossilize-log.txt and syslog.
function log() {
  # SteamGameId referenced but not assigned
  # shellcheck disable=SC2154
  echo "$(date -Iseconds) INFO $0/${SteamGameId}[$$]: $1" |
    tee -a "${FOSSILIZE_WRAP_LOG}"
  # TODO(b/203709231): Re-enable once properly filtered in feedback reports
  # echo "$0/${SteamGameId}[$$]: $1" | logger
}

function kill_fossilize_process() {
  local parent_pid="$1"
  local child_pids=""
  child_pids="$(ps -o pid= --ppid "${parent_pid}")"
  # SIGTERM for fossilize parent should kill the child processes upon successful
  # cleanup.
  kill -TERM "${parent_pid}"

  if [[ -z "${child_pids}" ]]; then
    # Child workers might have spawned between |child_pid| delcaration and
    # kill -TERM "${parent_pid}". Kill all fossilize replay process to be safe.
    pkill --full fossilize_replay
  else
    # If worker children are still alive, kill them. We must ensure they are not
    # running in the background before launching the game.
    for child_pid in ${child_pids}; do
      kill -KILL "${child_pid}"
    done
  fi
}

# STEAM_COMPAT_TRANSCODED_MEDIA_PATH is set by Steam
# shellcheck disable=SC2154
SteamGameId="${STEAM_COMPAT_TRANSCODED_MEDIA_PATH##*/}"

if ! [[ "${SteamGameId}" =~ ^[0-9]+$ ]]; then
  log "Invalid SteamGameId extracted"
fi

if [[ -n "${SOURCED_FOSSILIZE_OPTIONS}" ]]; then
  log "Read launch options from ${SOURCED_FOSSILIZE_OPTIONS}"
fi

# Dump environment prior to launching fossilize.
if [[ "${ENV_DUMP}" == "1" ]]; then
  log "Dumped environment to ${FOSSILIZE_WRAP_ENV}"
  env >"${FOSSILIZE_WRAP_ENV}"
fi

# Start fossilize process

# FOSSILIZE_REPLAY_WRAPPER needs to be unset (otherwise original wrapper
# simply calls this script again).
unset FOSSILIZE_REPLAY_WRAPPER

# FOSSILIZE_REPLAY_WRAPPER_ORIGINAL_APP is set by Steam
# shellcheck disable=SC2154
FOSSILIZE_CMD="${FOSSILIZE_REPLAY_WRAPPER_ORIGINAL_APP} ${FOSSILIZE_ARGS}"
log "Running command: ${FOSSILIZE_CMD}"

if [[ "${OUTPUT_DUMP}" == "1" ]]; then
  /bin/sh -c "${FOSSILIZE_CMD}" 2>&1 | tee "${FOSSILIZE_LOCAL_LOG}" &
else
  /bin/sh -c "${FOSSILIZE_CMD}" &
fi
FOSSILIZE_PID="$!"

if [[ "${DOWNLOAD_SHADER_CACHE}" == "0" ]]; then
  wait "${FOSSILIZE_PID}"
  RETVAL="$?"
  log "Command exit status: ${RETVAL}"
  exit "${RETVAL}"
fi

GARCON_PID=""
log "Running command to download shader cache"
# Create tee in subshell so that garcon command is spawned last. This ensures
# "$!" returns garcon call's PID.
LD_PRELOAD='' /opt/google/cros-containers/bin/garcon --client \
  --borealis-shader-cache --app-id="${SteamGameId}" --install --mount --wait \
  &> >(tee -a "${FOSSILIZE_WRAP_LOG}") &
GARCON_PID="$!"

# Wait for either one of the processes to finish
PID_DONE=""
wait -p PID_DONE -n "${FOSSILIZE_PID}" "${GARCON_PID}"
RETURN_CODE="$?"

if [[ "${PID_DONE}" == "${FOSSILIZE_PID}" ]]; then
  # Fossilize finished, shader cache download may be running
  if [[ "${RETURN_CODE}" == "0" ]]; then
    log "Fossilize success, not waiting for shader cache download but download"
    log "continues in the background"
    kill -KILL "${GARCON_PID}"
    RETVAL="0"
  else
    # Do not wait for shader cache background download to complete, the download
    # process is invisible to the end-user. Once download completes, it will be
    # mounted in the background.
    log "Fossilize returned ${RETURN_CODE}, launching the game and downloading"
    log "shader cache in the background"
    RETVAL="${RETURN_CODE}"
  fi
elif [[ "${PID_DONE}" == "${GARCON_PID}" ]]; then
  # Shader cache download finished, fossilize may be running
  if [[ "${RETURN_CODE}" == "0" ]]; then
    log "Shader cache download success, killing background fossilize process"
    kill_fossilize_process "${FOSSILIZE_PID}"
    RETVAL="0"
  else
    log "Shader cache download returned ${RETURN_CODE}, waiting for fossilize"
    wait "${FOSSILIZE_PID}"
    RETVAL="$?"
  fi
else
  log "Failed to wait for fossilize or shader cache download"
  log "Got pid: ${PID_DONE}, expected: ${FOSSILIZE_PID} or ${GARCON_PID}"
  RETVAL="255"
fi

log "Command exit status: ${RETVAL}"
exit "${RETVAL}"
