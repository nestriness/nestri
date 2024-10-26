#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple X11 window creation script

This script simply creates a window of a specified size and waits
for a key press to quit. The script can also be configured to dump
mouse click event information to the console with a command line
switch.
"""

import argparse
import sys

import Xlib.display
import Xlib.X
import Xlib.Xatom
import Xlib.Xutil


class Window:
    """The class which encapsulates the window details"""

    def __init__(self, width, height, capture_clicks, title, noborder):
        self.display = Xlib.display.Display()
        self.screen = self.display.screen()
        self.motif_wm_hints_atom = self.display.intern_atom("_MOTIF_WM_HINTS")

        # We use the key press event to quit the application in all cases
        event_mask = (
            Xlib.X.KeyPressMask
            | Xlib.X.KeyReleaseMask
            | Xlib.X.StructureNotifyMask
        )

        # If we are to dump click events we need to listen to button press events
        if capture_clicks:
            event_mask |= Xlib.X.ButtonPressMask | Xlib.X.ButtonReleaseMask

        self.window = self.screen.root.create_window(
            0,
            0,
            width,
            height,
            0,
            self.screen.root_depth,
            Xlib.X.InputOutput,
            Xlib.X.CopyFromParent,
            # special attribute mask
            background_pixel=self.screen.black_pixel,
            colormap=Xlib.X.CopyFromParent,
            event_mask=event_mask,
        )
        self.window.set_wm_name(title)

        self.window.map()
        if noborder:
            # We are sending the MOTIF_WM_HINTS change property command to the window manager
            # This command is sending (MWM_HINTS_DECORATIONS=0x2) and 3 zeroes
            # The remaining zeroes disable all the decorations (title bar, minimize, maximize, etc.)
            self.window.change_property(
                self.motif_wm_hints_atom,
                self.motif_wm_hints_atom,
                32,
                [0x2, 0x0, 0x0, 0x0],
            )
        self.display.flush()

    def event_loop(self):
        """The window event processing loop"""
        while True:
            event = self.display.next_event()

            if event.type == Xlib.X.DestroyNotify:
                sys.exit()
            elif event.type == Xlib.X.ButtonPress:
                print(f"up {event.event_x} {event.event_y}")
            elif event.type == Xlib.X.ButtonRelease:
                print(f"down {event.event_x} {event.event_y}")
            elif event.type == Xlib.X.KeyPress:
                sys.exit()


def main():
    """Main entry point of the direct scale testing script"""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "--width",
        default=640,
        type=int,
        help="Specify the width, in pixels, of the window to be created",
    )
    parser.add_argument(
        "--height",
        default=480,
        type=int,
        help="Specify the height, in pixels, of the window to be created",
    )
    parser.add_argument(
        "--clicks",
        action="store_true",
        help="Specifies whether mouse click events should be dumped to stdout",
    )
    parser.add_argument(
        "--noborder",
        action="store_true",
        help="Specify this flag to disable the window border",
    )
    parser.add_argument(
        "--fullscreen",
        action="store_true",
        help="[NOT IMPLEMENTED] Creates a fullscreen window",
    )
    parser.add_argument(
        "--title",
        default="XTEST",
        help="Allows the title of the window to be set to a custom name",
    )

    args = parser.parse_args()

    if not args.fullscreen:
        test_window = Window(
            args.width, args.height, args.clicks, args.title, args.noborder
        )
        test_window.event_loop()
    else:
        sys.exit("Fullscreen mode is not implemented")


if __name__ == "__main__":
    main()
