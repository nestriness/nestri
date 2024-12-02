#!/bin/bash
set -euo pipefail

# Wait for dbus socket to be ready
echo "Waiting for DBus system bus socket..."
DBUS_SOCKET="/run/dbus/system_bus_socket"
for _ in {1..10}; do # Wait up to 10 seconds
    if [ -e "$DBUS_SOCKET" ]; then
        echo "DBus system bus socket is ready."
        break
    fi
    sleep 1
done
if [ ! -e "$DBUS_SOCKET" ]; then
    echo "Error: DBus system bus socket did not appear. Exiting."
    exit 1
fi

# Wait for PipeWire to be ready
echo "Waiting for PipeWire socket..."
PIPEWIRE_SOCKET="/run/user/${UID}/pipewire-0"
for _ in {1..10}; do # Wait up to 10 seconds
    if [ -e "$PIPEWIRE_SOCKET" ]; then
        echo "PipeWire socket is ready."
        break
    fi
    sleep 1
done
if [ ! -e "$PIPEWIRE_SOCKET" ]; then
    echo "Error: PipeWire socket did not appear. Exiting."
    exit 1
fi

# Update system packages before proceeding
echo "Upgrading system packages..."
pacman -Syu --noconfirm

# GPU driver setup
echo "Detecting and installing appropriate GPU packages using chwd..."
chwd -a

echo "Detecting GPU vendor and installing necessary GStreamer plugins..."
source /etc/nestri/gpu_helpers.sh

get_gpu_info

# Identify vendor
if [[ "${vendor_full_map[0],,}" =~ "intel" ]]; then
    echo "Intel GPU detected. Installing GStreamer VAAPI and QSV plugins..."
    pacman -Syu --noconfirm gstreamer-vaapi gst-plugin-va gst-plugin-qsv
elif [[ "${vendor_full_map[0],,}" =~ "amd" ]]; then
    echo "AMD GPU detected. Installing GStreamer VAAPI plugins..."
    pacman -Syu --noconfirm gstreamer-vaapi gst-plugin-va
elif [[ "${vendor_full_map[0],,}" =~ "nvidia" ]]; then
    echo "NVIDIA GPU detected. NVENC plugins assumed to be in gst-plugins-bad"
else
    echo "Unknown GPU vendor. No additional GStreamer plugins installed"
fi

# Clean up remainders
echo "Cleaning up old package cache..."
paccache -rk1

echo "Switching to nestri user for application startup..."
exec sudo -E -u nestri /etc/nestri/entrypoint_nestri.sh
