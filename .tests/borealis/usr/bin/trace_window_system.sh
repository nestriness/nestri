#!/bin/bash
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Require two arguments.
if [[ $# -ne 2 || "$1" -eq "" ]]; then
    echo "Usage: $0 <X server process ID> <duration>"
    echo "    duration is formatted as per 'sleep' command."
    exit 1
fi

XPID="$1"
DURATION="${2}"

command -v strace >/dev/null 2>&1 || \
    { echo >&2 "Error: strace is not installed."; exit 1; }
command -v lsof >/dev/null 2>&1 || \
    { echo >&2 "Error: lsof is not installed."; exit 1; }
command -v toggle-wayland-debug >/dev/null 2>&1 || \
    { echo >&2 "Error: toggle-wayland-debug is not installed."; exit 1; }

# Set up for logging.
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
TEMPDIR="$(mktemp --directory --tmpdir "${TIMESTAMP}.XXXXXX")"

ARCHIVE_OUT="/tmp/windowtrace_${TIMESTAMP}.tar.gz"
LSOF_OUT="${TEMPDIR}/lsof.log"
PIDS_OUT="${TEMPDIR}/pids.log"
PROC_NET_UNIX_OUT="${TEMPDIR}/proc_net_unix.log"
STRACE_OUT="${TEMPDIR}/x_strace_pid${XPID}.log"
XLSATOMS_OUT="${TEMPDIR}/xlsatoms.log"

touch "${LSOF_OUT}" \
    "${PIDS_OUT}" \
    "${PROC_NET_UNIX_OUT}" \
    "${STRACE_OUT}"

PROC_NET_UNIX_STAGING="$(mktemp)"

# Invoking toggle-wayland-debug causes libwayland to log to files in
# /tmp/wayland-debug. See https://crrev.com/c/3008027.
# Borealis incorporates this patch by default. On other platforms, you'll need
# to manually build and install libwayland with that patch.
shopt -s nullglob
WAYLAND_LOGS=(/tmp/wayland-debug/*.log)

# Bundle up any extra logs produced by manual runs of strace/xtrace/etc.
# The trailing wildcard is for compatibility with `strace -ff`, which
# creates one file per process with a trailing ".<pid>" extension.
EXTRA_LOGS=(/tmp/trace_*.log*)

function cleanup {
    toggle-wayland-debug --disable
    kill "${LSOF_PID}"
    kill "${STRACE_PID}"
    kill "${PROC_NET_UNIX_PID}"

    # --force makes rm quiet if the file doesn't exist.
    rm --force "${PROC_NET_UNIX_STAGING}"

    # Log PIDs and cmdlines of traced processes.
    for f in "${XPID}" "${WAYLAND_LOGS[@]}"; do
        # Extract the PID from Wayland log filenames, which are similar to
        # "/tmp/wayland-debug/client-pid264-0x5ccf8b815c88.log".
        # This sed command is a no-op for ${XPID}, which is already a number.
        PID="$(sed "s/.*pid\([[:digit:]]\+\).*/\1/" <<< "${f}")"

        {
            echo -n "${PID}:"
            tr '\0' '\t' < "/proc/${PID}/cmdline"
            # Add a trailing newline since /proc/*/cmdline lacks one.
            echo
        } >> "${PIDS_OUT}"
    done

    # The X11 protocol uses ints to stand in for strings in many cases.
    # These int/string pairs are called atoms. They vary between runs and
    # are often important for understanding the semantics of various protocol
    # messages, so save a list of them.
    xlsatoms -display "${DISPLAY:-:0}" > "${XLSATOMS_OUT}"

    # Move all the logs into an archive.
    #
    # Truncate the wayland-debug logs instead of removing, as
    # they may still be open for writing. Ignore errors from truncate
    # in case there are no such logs.
    # TODO(cpelling): This sometimes zero-fills the start of the truncated
    # logs. We handle this in trace_window_system.py but it would be better
    # to avoid in the first place.
    tar --create \
        --file "${ARCHIVE_OUT}" \
        --gzip \
        --verbose \
        "${PIDS_OUT}" \
        "${STRACE_OUT}" \
        "${LSOF_OUT}" \
        "${PROC_NET_UNIX_OUT}" \
        "${XLSATOMS_OUT}" \
        "${WAYLAND_LOGS[@]}" \
        "${EXTRA_LOGS[@]}" \
        && rm -rf "${TEMPDIR}" \
        && (truncate --size=0 "${WAYLAND_LOGS[@]}" 2>/dev/null || true) \
        && echo "Success: Created ${ARCHIVE_OUT}" \
        && echo "If running a test image, download with:" \
        && echo "  ssh dut -- borealis-sh cat ${ARCHIVE_OUT} > ${ARCHIVE_OUT}"
}
trap cleanup EXIT

# Trace the X server's read/write syscalls.
# -ttt: System timestamps in seconds since epoch.
# -qq: Omit status and exit messages.
# -xx: All strings in hex (easier to convert to binary data later).
# -e: Syscalls used to read/write on Unix sockets.
# -yy: Map socket file descriptors to inodes. We can later recover
# process IDs/names from the inode numbers using lsof's output (see below).
# -s: Don't truncate long strings (so we can recover whole packets).
strace \
    -ttt \
    -qq \
    -xx \
    -e writev,recvmsg,recvfrom,sendmsg \
    -yy \
    -s 9999999 \
    --attach="${XPID}" \
    --output="${STRACE_OUT}" &
STRACE_PID=$!

# Generate information for mapping inodes to processes.
# -Fci: print command lines and inodes, in parseable form (-F)
# -n: Print numbers instead of network names.
# -P: Print numbers instead of port names.
# -l: Print user ID numbers instead of login names.
# -U: Select Unix socket domain files for printing.
# -a: "And" multiple args.
# -r10: Repeat every 10 seconds until killed.
lsof -Fci -n -P -l -U -a -r10 > "${LSOF_OUT}" 2>&1 &
LSOF_PID=$!

# Generate information for mapping inodes to sockets.
(
    while true; do
        # Read /proc/net/unix every couple of seconds, and write out the set of
        # unique sockets. Use a temp file as a staging area, to avoid a race.
        cat /proc/net/unix "${PROC_NET_UNIX_OUT}" | sort | uniq \
            > "${PROC_NET_UNIX_STAGING}"
        mv "${PROC_NET_UNIX_STAGING}" "${PROC_NET_UNIX_OUT}"
        sleep 2s
    done
) &
PROC_NET_UNIX_PID=$!

# Turn on Wayland debug (see comment above WAYLAND_LOGS).
toggle-wayland-debug --enable

echo "Tracing for ${DURATION}, press Ctrl-C to stop."
sleep "${DURATION}"
