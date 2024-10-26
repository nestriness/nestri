#!/usr/bin/env python3
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dump properties, size, and other basic info of all X windows."""

import os
import subprocess
import sys


def command_output(args, env):
    """Returns string output from the given command with given environment."""
    try:
        output = subprocess.check_output(args, env=env)
        return output.decode(sys.stdout.encoding, "backslashreplace")
    except Exception as e:  # pylint: disable=broad-except
        # Don't fail the entire script if one command fails.
        return repr(e)


def xrandr(env):
    """Returns a string with the output of the xrandr command."""
    return command_output(["xrandr"], env)


def root_xwininfo(env):
    """Returns a string showing the xwininfo tree."""
    return command_output(["xwininfo", "-tree", "-stats", "-root"], env)


def xwininfo(window_id, env, verbose):
    """Returns xwininfo for the given window_id."""
    if verbose:
        args = ["-all"]
    else:
        args = ["-stats"]
    return command_output(["xwininfo", "-id", window_id] + args, env)


def window_ids(info, verbose):
    """Parse window IDs from the xwininfo tree (see root_xwininfo())."""
    for line in info.splitlines():
        # Skip all of Steam's little unnamed windows, for brevity.
        if not verbose and ('(has no name): ("Steam" "Steam")' in line):
            continue

        l = line.lstrip()
        if l.startswith("0x"):
            yield l.split()[0]


def root_xprops(env):
    """Returns a string showing the X11 properties of the root window."""
    return command_output(["xprop", "-root", "-len", "1000"], env)


def xprops(window_id, env):
    """Returns a string showing the X11 properties of the given window."""
    # -len 1000 skips printing very long property values, like _NET_WM_ICON.
    return command_output(["xprop", "-id", window_id, "-len", "1000"], env)


def main():
    """Command-line routine."""
    # Use DISPLAY=:0 by default, but override with the system environment.
    # Therefore, if DISPLAY is set in the system environment, we'll use it.
    env = {"DISPLAY": ":0"}
    env.update(os.environ)

    verbose = "--verbose" in sys.argv

    info = root_xwininfo(env)

    print("=== Monitors ===")
    print(xrandr(env))

    print("=== Root window ===")
    print(info)
    print(root_xprops(env))

    for window_id in window_ids(info, verbose):
        print(f"=== Window {window_id} ===")
        print(xwininfo(window_id, env, verbose))
        print(xprops(window_id, env))


if __name__ == "__main__":
    main()
