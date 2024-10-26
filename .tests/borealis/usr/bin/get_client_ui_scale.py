#!/usr/bin/env python3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Prints the scale we are scaling the Steam client at.

It will be passed into the --forcedesktopscaling flag when starting the Steam
wrapper. A scale of 1 corresponds to a DPI of 130.
"""


import os
import re
import subprocess
import sys

def get_scale(display_info):
    """Parses output from xrandr and calculates scale."""
    first_display = display_info.splitlines()[1]
    # Display info is formatted as 0: +*DP-4 2560/597x1440/336+0+0  DP-4
    # so this will match 2560 and 597 corresponding to pixel width and physical
    # width in mm.
    match = re.search(r"([0-9]+)/([0-9]+)x", first_display)
    # Divide pixels by width in inches.
    dpi = float(match[1]) / (float(match[2]) / 25.4)
    # Using a DPI of 130 as scale of 1.
    scale = dpi / 130.0
    return scale


def main():
    try:
        # Use DISPLAY=:0 by default, but override with the system environment.
        # Therefore, if DISPLAY is set in the system environment, we'll use it.
        env = {"DISPLAY": ":0"}
        env.update(os.environ)
        output = subprocess.check_output(
            ["xrandr", "--listmonitors"],
            encoding="utf-8",
            env=env,
        )
        print(get_scale(output))
    # If error default to scale of 1.
    except Exception as e:
        print(1)
        print(e, file=sys.stderr)


if __name__ == "__main__":
    main()
