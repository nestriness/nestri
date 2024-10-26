#!/bin/bash

# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Wrapper for launching Steam games with additional tracing, logging and
# configuration.  To configure, create /tmp/launch-options.sh and set the
# environment variables listed below.

USERNAME="chronos"

CONFIG_DIR="/home/${USERNAME}/.config/launch-wrap"
CACHE_DIR="/home/${USERNAME}/.cache/launch-wrap"

# Allow overriding /tmp in unit tests.
: "${LAUNCH_WRAP_TMP:=/tmp}"

: "${OUTPUT_DIR:="${LAUNCH_WRAP_TMP}"}"
if [[ -f "${CONFIG_DIR}/persistent-logs" ]]; then
  if [[ ! -d "${CACHE_DIR}" ]]; then
    # When this script runs, /home/${USERNAME}/.cache should already be created
    # by maitred's setup-cache-dir.
    mkdir -m 0755 "${CACHE_DIR}"
    chown "${USERNAME}:${USERNAME}" "${CACHE_DIR}"
  fi
  OUTPUT_DIR="${CACHE_DIR}"
fi

# Read options that were placed in a launch options script.
: "${LAUNCH_OPTIONS:="${LAUNCH_WRAP_TMP}/launch-options.sh"}"
if [[ -f "${LAUNCH_OPTIONS}" ]]; then
  # ShellCheck can't follow non-constant source.
  # shellcheck disable=SC1090
  source "${LAUNCH_OPTIONS}"
  SOURCED_LAUNCH_OPTIONS="$(cat "${LAUNCH_OPTIONS}")"
else
  SOURCED_LAUNCH_OPTIONS=""
fi

# Configuration that can be updated via environment setting or modifications
# to the file $LAUNCH_OPTIONS.
: "${STRACE:="0"}"
: "${XTRACE:="0"}"
: "${APITRACE:="0"}"
: "${ZINK:="0"}"
: "${MANGOHUD:="0"}"
: "${ENV_DUMP:="0"}"
: "${OUTPUT_DUMP:="0"}"
: "${PROTON_DEBUG:="0"}"
: "${PROTON_ESYNC:="0"}"
: "${PROTON_HEAP_VALIDATION:="0"}"
: "${PROTON_SYNC_X11:="0"}"
: "${PROTON_WINESERVER_SYNC:="0"}"
: "${PRESSURE_VESSEL_DEBUG:="0"}"
: "${GAME_OVERRIDES:="1"}"
: "${INSIGHT_LAYER:="1"}"
: "${VENUS_DUMP:="0"}"
: "${VENUS_DEBUG:="0"}"
: "${MESA_PREFIX:=""}"
: "${MOUNT_PRECOMPILED_SHADER_CACHE:="1"}"
: "${LOG_FILE:="${OUTPUT_DIR}/launch-log.txt"}"
: "${STRACE_FILE:="${OUTPUT_DIR}/game.strace"}"
: "${XTRACE_FILE:="${OUTPUT_DIR}/game.xtrace"}"
: "${APITRACE_FILE:="${OUTPUT_DIR}/game.trace"}"
: "${MANGOHUD_LOG_FOLDER:="${OUTPUT_DIR}/mangohud-log"}"
: "${VENUS_FILE:="${OUTPUT_DIR}/game.venus"}"
: "${ENV_FILE:="${OUTPUT_DIR}/game.env"}"
: "${OUTPUT_FILE:="${OUTPUT_DIR}/game.log"}"
: "${INSIGHT_LAYER_ENGINE_FILE:="${OUTPUT_DIR}/engine.log"}"

# Log to /tmp/launch-log.txt and syslog.
function log() {
  # NOTE: Changes to the log format below must be reflected in
  # unit tests and in /usr/bin/get_compat_tool_versions.py

  local now
  now="$(date -Iseconds)"

  # SteamGameId referenced but not assigned
  # shellcheck disable=SC2154
  echo -e "${now} INFO $0/${SteamGameId}[$$]: $1" | tee -a "${LOG_FILE}"
  # TODO(b/203709231): Re-enable once properly filtered in feedback reports
  # echo "$0/${SteamGameId}[$$]: $1" | logger
}

# Used by scripts in /etc/launch-wrapper.d.
# Adds argument $1 to the end of $CMD if it is not already present, and $CMD
# does not match regular expressions $2, $3, ...
function add_cmd_suffix_if_not_present() {
  if [[ "${CMD} " == *" $1 "* ]]; then
    log "Not adding $1 to the launch command, as it has already been \
specified by the user."
    return
  fi
  for denyvalue in "${@:2}"; do
    local re="( )${denyvalue}(\$| )"
    if [[ "${CMD}" =~ ${re} ]]; then
      log "Not adding $1 to the launch command, as ${denyvalue} has been \
specified by the user."
      return
    fi
  done
  log "Adding $1 to launch command."
  CMD="${CMD} $1"
}

function runtime_is_proton() {
  [[ "${CMD}" == *"Proton"* ]]
  return $?
}

function runtime_is_slr() {
  ! runtime_is_proton && [[ "${CMD}" == *"SteamLinuxRuntime"* ]]
  return $?
}

function device_is_intel() {
  grep -F -q "Intel(R)" /proc/cpuinfo
  return $?
}

function experimental_override_compat_tool() {
  # Override compat tool as Proton if the user has not specified any.
  if [[ -z "$(get_compat_tool.py --game-id "${SteamGameId}")" ]]; then
    log "Overriding compat tool as $1"
    touch /tmp/specifycompattoolfile
    steam "steam://specifycompattool/${SteamGameId}/$1"

    # Keep track of this change in a file
    mkdir -p "${HOME}/.local/share/borealis-compat-tool"
    echo "$1" >"${HOME}/.local/share/borealis-compat-tool/${SteamGameId}"

    # Re-launch the game. Because Steam has updates to perform upon compat
    # tool change (even if Proton is installed), we can simply queue the
    # game update&run then exit to effectively relaunch the game.
    steam "steam://rungameid/${SteamGameId}"
    exit
  fi
}

CMD="$*"

if [[ -n "${SOURCED_LAUNCH_OPTIONS}" ]]; then
  log "Read launch options from ${LAUNCH_OPTIONS}"
  log "Launch options read-in:\n${SOURCED_LAUNCH_OPTIONS}"
fi

# Mesa override by setting $MESA_PREFIX.
if [[ -n "${MESA_PREFIX}" ]]; then
  # Use absolute paths
  MESA_ABS_PREFIX="$(realpath "${MESA_PREFIX}")"
  log "Using mesa from ${MESA_ABS_PREFIX}"
  for lib_dir in lib lib32; do
    LIB_PATH="${MESA_ABS_PREFIX}/${lib_dir}"
    export LD_LIBRARY_PATH="${LIB_PATH}:${LD_LIBRARY_PATH}"
    export LIBGL_DRIVERS_PATH="${LIB_PATH}/dri:${LIBGL_DRIVERS_PATH}"
  done
  for arch in x86_64 i386; do
    ICD_FILE="${MESA_ABS_PREFIX}/share/vulkan/icd.d/virtio_icd.${arch}.json"
    export VK_ICD_FILENAMES="${ICD_FILE}:${VK_ICD_FILENAMES}"
  done
fi

# Give Proton (wine) a fake PulseAudio socket, so it will fallback
# to use ALSA device directly. This fallback is handled by wine.
if runtime_is_proton; then
  export PULSE_SERVER=/dev/null
fi

if runtime_is_proton || runtime_is_slr; then
  export STEAM_COMPAT_MOUNTS="${STEAM_COMPAT_MOUNTS}:/run/perfetto"
fi

# TODO(b/324943871#comment69): Remove once ANV shader compiler performance is
# improved: https://gitlab.freedesktop.org/mesa/mesa/-/issues/9288
#
# DXVK creates a pipeline library compilation thread per HW thread by default.
# On Intel devices, this results in the ANV shader compiler taking up too much
# CPU time. We halve the max number of threads to give the game more CPU time.
#
# The minimum PROCESSOR_COUNT is expected to be 4. However, even if it is less
# than that, there is no need to guard against MAX_DXVK_COMPILER_THREADS = 0
# because a value of 0 prompts DXVK to use the maximum HW concurrency.
if device_is_intel; then
  PROCESSOR_COUNT="$(nproc --all)"
  MAX_DXVK_COMPILER_THREADS="$(( "${PROCESSOR_COUNT}" / 2 ))"
  export DXVK_CONFIG="dxvk.numCompilerThreads = ${MAX_DXVK_COMPILER_THREADS};"
fi

# Per-game overrides.
if [[ "${GAME_OVERRIDES}" -eq 1 ]]; then
  # /etc/launch-wrapper.d/${SteamGameId}.sh will execute game-specific
  # fixup code, which might edit ${CMD} or change game config/data files.
  OVERRIDE_FILE="/etc/launch-wrapper.d/${SteamGameId}.sh"
  if [ -f "${OVERRIDE_FILE}" ]; then
    # ShellCheck can't follow non-constant source.
    # shellcheck disable=SC1090
    source "${OVERRIDE_FILE}"
  fi
fi

if [[ "${VENUS_DUMP}" -eq 1 ]]; then
  : "${VN_DEBUG:="wsi,log_ctx_info"}"
  export VN_DEBUG
fi

if [[ "${VENUS_DEBUG}" -eq 1 ]]; then
  : "${VN_PERF:="all"}"
  export VN_PERF
fi

# Proton logging.
if [[ "${PROTON_DEBUG}" -eq 1 ]]; then
  export PROTON_LOG=1

  # Proton default options
  export WINEDEBUG="${WINEDEBUG}",+timestamp,+pid,+tid,+seh,+unwind,+threadname
  WINEDEBUG="${WINEDEBUG}",+debugstr,+loaddll,+mscoree
  export VKD3D_SHADER_DEBUG=fixme
  export WINE_MONO_TRACE=E:System.NotImplementedException

  # Add-ons for Borealis debugging
  export DXVK_LOG_LEVEL=debug
  export VKD3D_DEBUG=trace
  WINEDEBUG="${WINEDEBUG}",+d3d,+vulkan,+opengl,+opencl # Graphics/GPU
  WINEDEBUG="${WINEDEBUG}",+x11drv,+message             # Windowing
  WINEDEBUG="${WINEDEBUG}",+server,+client              # IPC/Events
  WINEDEBUG="${WINEDEBUG}",+module,+loaddll             # Loader
  WINEDEBUG="${WINEDEBUG}",+msgbox                      # Game error msgs
fi

if [[ "${PROTON_HEAP_VALIDATION}" -eq 1 ]]; then
  # Forces Wine to validate heap contents with every access.
  # Does not log calls to allocate/free.
  export WINEDEBUG="${WINEDEBUG}",warn+heap
fi

if [[ "${PROTON_SYNC_X11}" -eq 1 ]]; then
  # Forces the X11 driver in Proton into synchronous mode.
  export WINEDEBUG="${WINEDEBUG}",+synchronous
fi

if [[ "${PROTON_ESYNC}" -eq 1 ]]; then
  # Forces Proton to use esync (fsync disabled).
  export PROTON_NO_FSYNC=1
fi

if [[ "${PROTON_WINESERVER_SYNC}" -eq 1 ]]; then
  # Forces Proton to use wineserver syncs (fsync and esync disabled).
  export PROTON_NO_FSYNC=1
  export PROTON_NO_ESYNC=1
fi

# Pressure-Vessel logging.
if [[ "${PRESSURE_VESSEL_DEBUG}" -eq 1 ]]; then
  export PRESSURE_VESSEL_VERBOSE=1
  export PRESSURE_VESSEL_LOG_INFO=1
  export PRESSURE_VESSEL_LOG_WITH_TIMESTAMP=1
  export G_MESSAGE_DEBUG=all
fi

# Dump environment prior to launching the game.
if [[ "${ENV_DUMP}" -eq 1 ]]; then
  log "Dumped environment to ${ENV_FILE}"
  env >"${ENV_FILE}"
fi

# Update command line for tracing, quoting paths with bash's @Q parameter
# expansion, which quotes a string "in a format that can be reused as input".
if [[ "${STRACE}" -eq 1 ]]; then
  CMD="strace -o ${STRACE_FILE@Q} -s 256 -f -ttT ${CMD}"
fi
if [[ "${XTRACE}" -eq 1 ]]; then
  CMD="x11trace -n -o ${XTRACE_FILE} -- ${CMD}"
fi
if [[ "${APITRACE}" -eq 1 ]]; then
  CMD="apitrace trace -o ${APITRACE_FILE@Q} ${CMD}"
fi

# Zink is enabled based on a finch/chrome flag. The flag has parameters
# ZinkEnableRecommended or ZinkEnableAll. ZinkEnableRecommended will enabled zink only
# on recommended games where ZINK=1 is set in the override file. ZinkEnableAll
# will enable zink for all games.
ZINK_FLAG_DIR="${HOME}/.borealis_flags/BorealisZinkGlDriver"
if [[ -f "${ZINK_FLAG_DIR}/ZinkEnableAll" ]] ||
  { [[ -f "${ZINK_FLAG_DIR}/ZinkEnableRecommended" ]] &&
    [[ "${ZINK}" -eq 1 ]]; }; then
  log "Enabling zink"
  CMD="MESA_LOADER_DRIVER_OVERRIDE=zink GALLIUM_DRIVER=zink ${CMD}"
fi

# Update command line for mangohud support including logging
if [[ "${MANGOHUD}" -eq 1 ]]; then
  mkdir -p "${MANGOHUD_LOG_FOLDER}"
  CMD="MANGOHUD_CONFIG=output_folder=${MANGOHUD_LOG_FOLDER@Q} MANGOHUD=1 ${CMD}"
fi

# Enable insight layer
if [[ "${INSIGHT_LAYER}" -eq 1 ]]; then
  export VK_INSTANCE_LAYERS="VK_LAYER_INSIGHT_layer:${VK_INSTANCE_LAYERS}"
  CMD="INSIGHT_LAYER_ENGINE_FILE=${INSIGHT_LAYER_ENGINE_FILE@Q} ${CMD}"
fi

log "Running command: ${CMD}"

if [[ "${MOUNT_PRECOMPILED_SHADER_CACHE}" -eq 1 ]]; then
  # Queue mount before game launch
  LD_PRELOAD='' /opt/google/cros-containers/bin/garcon --client \
    --borealis-shader-cache --app-id="${SteamGameId}" --install --mount \
    &> >(tee -a "${LOG_FILE}")
fi

if [[ "${VENUS_DUMP}" -eq 1 ]]; then
  if [[ "${OUTPUT_DUMP}" -eq 1 ]]; then
    /bin/sh -c "${CMD}" 2>&1 |
      tee "${OUTPUT_FILE}" |
      tee >(stdbuf -i0 -o0 -e0 grep MESA-VIRTIO >"${VENUS_FILE}")
  else
    /bin/sh -c "${CMD}" 2>&1 |
      tee >(stdbuf -i0 -o0 -e0 grep MESA-VIRTIO >"${VENUS_FILE}")
  fi
else
  if [[ "${OUTPUT_DUMP}" -eq 1 ]]; then
    /bin/sh -c "${CMD}" &>"${OUTPUT_FILE}"
  else
    /bin/sh -c "${CMD}"
  fi
fi
RETVAL="$?"

if [[ "${MOUNT_PRECOMPILED_SHADER_CACHE}" -eq 1 ]]; then
  # Queue unmount after game exits
  LD_PRELOAD='' /opt/google/cros-containers/bin/garcon --client \
    --borealis-shader-cache --app-id="${SteamGameId}" --unmount \
    &> >(tee -a "${LOG_FILE}")
fi

log "Command exit status: ${RETVAL}"
exit "${RETVAL}"
