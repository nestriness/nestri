#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

log() {
  logger --stderr --tag "steam-wrapper/migrate_shadercache" "$@"
}

# Absolute paths to Steam's shadercache directory and mounted path
shadercache_path="$1"
mounted_shadercache_path="$2"

if [[ ! -d "${shadercache_path}" ]]; then
  log "Steam shadercache path does not exist: ${shadercache_path}"
  exit 1
fi

if [[ ! -d "${mounted_shadercache_path}" ]]; then
  log "Mounted shadercache does not exist: ${mounted_shadercache_path}"
  exit 1
fi

real_shadercache_path="$(realpath "${shadercache_path}")"
if [[ "${real_shadercache_path}" == "${mounted_shadercache_path}" ]]; then
  # Nothing to do, directory has already been migrated
  exit 0
fi

if [[ -z "$(ls "${shadercache_path}")" ]]; then
  log "No migration required, shadercache directory is empty"
  rmdir "${shadercache_path}"
  ln -s "${mounted_shadercache_path}" "${shadercache_path}"
  exit 0
fi

# Check if we are safe to migrate

# Find processes that contains 'steam'. Only "steam-runtime" should exist or
# none (launch directly). Matching 'steam' ensures we detect any Steam processes
# including steam.sh, steamwebhelper, steam.
pgrep_steam="$(pgrep --count "steam")"
if [[ ${pgrep_steam} -gt 1 ]]; then
  # This is triggered if user has not migrated yet but 'closes' Steam then
  # launch via shelf. We don't want to output to logger every time, so we only
  # echo for development purposes.
  echo "Steam is running"
  exit 1
fi

file_name="$(basename "$0")"
# Must be 1 (cannot be 0). Match --full so that we match /bin/bash <script>.
pgrep_self="$(pgrep --full --count "${file_name}")"
if [[ ${pgrep_self} -gt 1 ]]; then
  # Advanced users might be able to figure out running this script directly.
  log "Migration is already ongoing"
  exit 114 #EALREADY
fi

# Since this script is intended to be called by steam-runtime, we can make a
# safe assumption that Steam is not running and will not be launched from this
# point onwards (unless advanced users launch the script then click on Steam
# in launcher). Safe to perform migration.

BASE_TEXT="Optimizing Steam game performance (one-time setup).."
# awk to extract out:
#  - $2 - % file transfer progress
#  - $1 - file name if there was no $3 match
#  - $3 - transfer speed
# zenity requires % integer progress to be printed as-is with new line, and
# Information text to be printed with '#' prefix.
# Examples:
#   line  : sending incremental file list
#   result: <none>
#   line  : sent 117,874,930 bytes  received 85 bytes  78,583,343.33 bytes/sec
#   result: <none>
#   line  : total size is 117,845,720  speedup is 1.00
#   result: <none>
#   line  : 620/fozpipelinesv6/foz_cache.foz
#   result: no output, filename set as line input
#   line  : 117,740,928  99%  748.37MB/s    0:00:00 (xfr#1, to-chk=1/9)
#   result: output two lines:
#             99
#             # <BASE_TEXT>\n\n<filename>\n748.37MB/s
# shellcheck disable=SC2016
awk_arg='/^ /
{ print int(+$2) }
$1 { if(!$3) filename=$1 }
{ if(filename=="sending" || filename=="total" || filename=="sent" || filename=="building") next }
$3 { if(int(+$3)) print "# " "'"${BASE_TEXT}"'\\n\\n" filename "\\n" $3; fflush() }'

echo "Migrating files.."

rsync_pid_file="/tmp/shadercahe-migration-rsync-pid"
# Notes:
# - stdbuf to set pipe buffer size to zero
# - Allow people to cancel the migration process (continue launching Steam)
# shellcheck disable=SC2094
(
  rsync -av --remove-source-files --info=progress2 \
    "${shadercache_path}/" "${mounted_shadercache_path}/" &
  echo "$!" >&3
) 3>"${rsync_pid_file}" |
  stdbuf -i0 -o0 -e0 tr '\r' '\n' |
  stdbuf -i0 -o0 -e0 awk "${awk_arg}" |
  {
    zenity --progress \
      --title="${BASE_TEXT}" \
      --auto-close \
      --cancel-label=Skip \
      --time-remaining \
      --text="${BASE_TEXT}\n\n\n" \
      --display=:0 \
      --percentage=0

    # Very long commands makes SC2181 unfeasible - code is harder to read
    # shellcheck disable=SC2181
    if [[ "$?" != "0" ]]; then
      # Kill rsync if zenity exited before reaching 100% (user cancelled or
      # unexpected failure)
      # zenity --auto-kill does not actually kill rsync process, hence we
      # manually kill rsync on zenity exit
      kill -9 "$(<"${rsync_pid_file}")"
    fi
    rm "${rsync_pid_file}"
  }

# Clean up empty directories after migration
find "${shadercache_path}" -type d -empty -delete

if [[ ! -d "${shadercache_path}" ]] &&
  ln -s "${mounted_shadercache_path}" "${shadercache_path}"; then
  log "One-time migration completed!"
  exit 0
fi

log "One-time migration cancelled by user or failed"
exit 1
