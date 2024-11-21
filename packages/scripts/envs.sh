#!/bin/bash -e

export XDG_RUNTIME_DIR=/run/user/${UID}/
export WAYLAND_DISPLAY=wayland-0
export DISPLAY=:0
export $(dbus-launch)

# Fixes freezing issue
export PROTON_NO_FSYNC=1

# Our preferred prefix
export WINEPREFIX=${USER_HOME}/.nestripfx/
