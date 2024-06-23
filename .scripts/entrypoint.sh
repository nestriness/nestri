#!/bin/bash -e
trap "echo TRAPed signal" HUP INT QUIT TERM

# Include our gpu helper functions
source /etc/gpu_helpers.sh

# Create and modify permissions of XDG_RUNTIME_DIR
sudo -u nestri mkdir -pm700 /tmp/runtime-1000
sudo chown nestri:nestri /tmp/runtime-1000
sudo -u nestri chmod 700 /tmp/runtime-1000
# Make user directory owned by the user in case it is not
sudo chown nestri:nestri /home/nestri || sudo chown nestri:nestri /home/nestri/* || { echo "$(date +"[%Y-%m-%d %H:%M:%S]") Failed to change user directory permissions. There may be permission issues."; }

#Input devices ownable by our default user
export REQUIRED_DEVICES=${REQUIRED_DEVICES:-/dev/uinput /dev/input/event*}

declare -A group_map

for dev in $REQUIRED_DEVICES; do
  if [ -e "$dev" ]; then
    dev_group=$(stat -c "%G" "$dev")
    dev_gid=$(stat -c "%g" "$dev")

    if [ "$dev_group" = "UNKNOWN" ]; then
      new_name="nestri-gid-$dev_gid"
      # We only have a GID for this group; create a named group for it
      # this isn't 100% necessary but it prevents some useless noise in the console
      sudo groupadd -g "$dev_gid" "$new_name"
      group_map[$new_name]=1
    else
      # the group already exists; just add it to the list
      group_map[$dev_group]=1
    fi

    # is this device read/writable by the group? if not, make it so
    if [ "$(stat -c "%a" "$dev" | cut -c2)" -lt 6 ]; then
      sudo chmod g+rw "$dev"
    fi
  else
    echo "$(date +"[%Y-%m-%d %H:%M:%S]") Path '$dev' is not present."
  fi
done

join_by() { local IFS="$1"; shift; echo "$*"; }

groups=$(join_by "," "${!group_map[@]}")
if [ "$groups" != "" ]; then
  echo "$(date +"[%Y-%m-%d %H:%M:%S]") Adding user '${USER}' to groups: $groups"
  sudo usermod -a -G "$groups" "${USER}"
else
  echo "$(date +"[%Y-%m-%d %H:%M:%S]") Not modifying user groups ($groups)"
fi

# Remove directories to make sure the desktop environment starts
sudo rm -rf /tmp/.X* ~/.cache
# Change time zone from environment variable
sudo ln -snf "/usr/share/zoneinfo/$TZ" /etc/localtime && echo "$TZ" | sudo tee /etc/timezone >/dev/null
# Add gamescope directories to path
export PATH="${PATH:+${PATH}:}/usr/local/games:/usr/games"

# This symbolic link enables running Xorg inside a container with `-sharevts`
sudo ln -snf /dev/ptmx /dev/tty7
# Start DBus without systemd
sudo /etc/init.d/dbus start

# Install Proton-GE for this user
nestri-proton -i

# Allow starting Xorg from a pseudoterminal instead of strictly on a tty console
if [ ! -f "/etc/X11/Xwrapper.config" ]; then
    echo -e "allowed_users=anybody\nneeds_root_rights=yes" | sudo tee /etc/X11/Xwrapper.config > /dev/null
fi
if grep -Fxq "allowed_users=console" /etc/X11/Xwrapper.config; then
  sudo sed -i "s/allowed_users=console/allowed_users=anybody/;$ a needs_root_rights=yes" /etc/X11/Xwrapper.config
fi

# Remove existing Xorg configuration
if [ -f "/etc/X11/xorg.conf" ]; then
  sudo rm -f "/etc/X11/xorg.conf"
fi

# Setting `VIDEO_PORT` to none disables RANDR/XRANDR, do not set this if using datacenter GPUs
if [ "${VIDEO_PORT,,}" = "none" ]; then
  export CONNECTED_MONITOR="--use-display-device=None"
# The X server is otherwise deliberately set to a specific video port despite not being plugged to enable RANDR/XRANDR, monitor will display the screen if plugged to the specific port
else
  export CONNECTED_MONITOR="--connected-monitor=${VIDEO_PORT}"
fi

# A custom modeline should be generated because there is no monitor to fetch this information normally
custom_modeline="$(cvt -r "${SIZEW}" "${SIZEH}" "${REFRESH}" | sed -n 2p)"
custom_modeline_settings="$(echo "$custom_modeline" | sed 's/Modeline //')"
custom_modeline_identifier="$(echo "$custom_modeline_settings" | grep -o '"[^"]*"')"

# Pre-populate GPU information manually
if ! check_and_populate_gpus; then
  exit 1
fi

# Select the GPU based on user input or first one available
selected_gpu="${GPU_SELECTION,,:-}"
if [[ -z "$selected_gpu" ]]; then
  selected_gpu="${gpu_map[0]}" # Select first available GPU
  echo "No GPU selected, using first one available: $selected_gpu"
elif ! selected_gpu=$(check_selected_gpu "$selected_gpu"); then
  exit 1
fi

# Print selected GPU information
echo "Selected GPU: $(print_gpu_info "$selected_gpu")"
echo ""

# Get GPU vendor as separate variable
selected_gpu_vendor=$(get_gpu_vendor "$selected_gpu")
# Convert lshw gathered bus id into Xorg compatible one
xorg_bus_id=$(get_gpu_bus_xorg "$selected_gpu")

# Check if the selected GPU is an NVIDIA GPU
if [[ "${selected_gpu_vendor,,}" =~ "nvidia" ]]; then
    echo "Selected GPU is NVIDIA. Handling NVIDIA-specific configuration..."

    # Install NVIDIA userspace driver components including X graphic libraries
    if ! command -v nvidia-xconfig &> /dev/null; then
      # Driver version is provided by the kernel through the container toolkit
      export DRIVER_ARCH="$(dpkg --print-architecture | sed -e 's/arm64/aarch64/' -e 's/armhf/32bit-ARM/' -e 's/i.*86/x86/' -e 's/amd64/x86_64/' -e 's/unknown/x86_64/')"
      export DRIVER_VERSION="$(head -n1 </proc/driver/nvidia/version | awk '{for(i=1;i<=NF;i++) if ($i ~ /^[0-9]+\.[0-9\.]+/) {print $i; exit}}')"
      cd /tmp
      # If version is different, new installer will overwrite the existing components
      if [ ! -f "/tmp/NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}.run" ]; then
        # Check multiple sources in order to probe both consumer and datacenter driver versions
        curl -fsSL -O "https://international.download.nvidia.com/XFree86/Linux-${DRIVER_ARCH}/${DRIVER_VERSION}/NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}.run" || curl -fsSL -O "https://international.download.nvidia.com/tesla/${DRIVER_VERSION}/NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}.run" || { echo "Failed NVIDIA GPU driver download. Exiting."; exit 1; }
      fi
      # Extract installer before installing
      sudo sh "NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}.run" -x
      cd "NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}"
      # Run installation without the kernel modules and host components
      sudo ./nvidia-installer --silent \
                        --no-kernel-module \
                        --install-compat32-libs \
                        --no-nouveau-check \
                        --no-nvidia-modprobe \
                        --no-rpms \
                        --no-backup \
                        --no-check-for-alternate-installs
      sudo rm -rf /tmp/NVIDIA* && cd ~
    fi

    # Generate /etc/X11/xorg.conf with nvidia-xconfig
    sudo nvidia-xconfig --virtual="${SIZEW}x${SIZEH}" --depth="$CDEPTH" --mode="$(echo "$custom_modeline" | awk '{print $2}' | tr -d '\"')" --allow-empty-initial-configuration --no-probe-all-gpus --busid="$xorg_bus_id" --include-implicit-metamodes --mode-debug --no-sli --no-base-mosaic --only-one-x-screen ${CONNECTED_MONITOR}
    # Guarantee that the X server starts without a monitor by adding more options to the configuration
    sudo sed -i '/Driver\s\+"nvidia"/a\    Option         "ModeValidation" "NoMaxPClkCheck,NoEdidMaxPClkCheck,NoMaxSizeCheck,NoHorizSyncCheck,NoVertRefreshCheck,NoVirtualSizeCheck,NoExtendedGpuCapabilitiesCheck,NoTotalSizeCheck,NoDualLinkDVICheck,NoDisplayPortBandwidthCheck,AllowNon3DVisionModes,AllowNonHDMI3DModes,AllowNonEdidModes,NoEdidHDMI2Check,AllowDpInterlaced"' /etc/X11/xorg.conf

    # Add custom generated modeline to the configuration
    sudo sed -i '/Section\s\+"Monitor"/a\    '"$custom_modeline" /etc/X11/xorg.conf
    # Prevent interference between GPUs, add this to the host or other containers running Xorg as well
    echo -e "Section \"ServerFlags\"\n    Option \"AutoAddGPU\" \"false\"\nEndSection" | sudo tee -a /etc/X11/xorg.conf > /dev/null
else
    echo "Selected GPU is non-NVIDIA. Handling common configuration..."

    # We need permissions for the GPU(s)
    sudo chown -R root:root /dev/dri/*
    sudo chmod -R 777 /dev/dri/*

    # Create common config file
    sudo touch /etc/X11/xorg.conf
    config_common_xorg="
Section \"ServerLayout\"
    Identifier     \"Layout0\"
    Screen      0  \"Screen0\"
    InputDevice    \"Keyboard0\" \"CoreKeyboard\"
    InputDevice    \"Mouse0\" \"CorePointer\"
EndSection

Section \"InputDevice\"
    Identifier     \"Mouse0\"
    Driver         \"mouse\"
    Option         \"Protocol\" \"auto\"
    Option         \"Device\" \"/dev/mouse\"
    Option         \"Emulate3Buttons\" \"no\"
    Option         \"ZAxisMapping\" \"4 5\"
EndSection

Section \"InputDevice\"
    Identifier     \"Keyboard0\"
    Driver         \"kbd\"
EndSection

Section \"Device\"
    Identifier     \"Device0\"
    Driver         \"modesetting\"
    Option         \"AsyncFlipSecondaries\" \"yes\"
    BusID          \"$xorg_bus_id\"
EndSection

Section \"Screen\"
    Identifier     \"Screen0\"
    Device         \"Device0\"
    DefaultDepth    $CDEPTH
    Option         \"ModeDebug\" \"True\"
    SubSection     \"Display\"
        Virtual     ${SIZEW} ${SIZEH}
        Depth       $CDEPTH
        Modes       $custom_modeline_identifier
    EndSubSection
EndSection

Section \"ServerFLags\"
    Option \"AutoAddGPU\" \"off\"
EndSection
"
    echo "$config_common_xorg" | sudo tee /etc/X11/xorg.conf > /dev/null
fi

# Default display is :0 across the container
export DISPLAY=":0"
# Run Xorg server with required extensions
/usr/bin/Xorg vt7 -noreset -novtswitch -sharevts -dpi "${DPI}" +extension "COMPOSITE" +extension "DAMAGE" +extension "GLX" +extension "RANDR" +extension "RENDER" +extension "MIT-SHM" +extension "XFIXES" +extension "XTEST" "${DISPLAY}" &

# Wait for X11 to start
echo "Waiting for X socket"
until [ -S "/tmp/.X11-unix/X${DISPLAY/:/}" ]; do sleep 1; done
echo "X socket is ready"

# Wait for X11 to start
echo "$(date +"[%Y-%m-%d %H:%M:%S]") Waiting for X socket"
until [ -S "/tmp/.X11-unix/X${DISPLAY/:/}" ]; do sleep 1; done
echo "$(date +"[%Y-%m-%d %H:%M:%S]") X socket is ready"

# Additional non-NVIDIA configuration required
if [[ ! "${selected_gpu_vendor,,}" =~ "nvidia" ]]; then
  echo "Creating new output mode: '$custom_modeline_settings'"
  xrandr --newmode $custom_modeline_settings

  # Look for an output for our shenanigans
  # If someone has dummy plugs or has a screen connected, use connected output to prevent resolution issues
  selected_output="$(xrandr --query | grep -Eo 'HDMI-[0-9]+ connected' | head -1 | awk '{print $1}')"
  if [ -z "$selected_output" ]; then
    selected_output="$(xrandr --query | grep -Eo 'DP-[0-9]+ connected' | head -1 | awk '{print $1}')"
  fi

  # Look for disconnected output otherwise
  # priorizing "HDMI-n" outputs as "DP-n" ones seem to have trouble with configuring output multiple times on Intel GPUs
  if [ -z "$selected_output" ]; then
    echo "Could not find a connected output, looking for a disconnected one"
    selected_output="$(xrandr --query | grep -Eo 'HDMI-[0-9]+ disconnected' | head -1 | awk '{print $1}')"
    if [ -z "$selected_output" ]; then
      selected_output="$(xrandr --query | grep -Eo 'DP-[0-9]+ disconnected' | head -1 | awk '{print $1}')"
    fi
  fi

  # Sanity check
  if [ -z "$selected_output" ]; then
    echo "Could not find an usable output"
    exit 1
  fi

  echo "Configuring output: '$selected_output' to use mode '$custom_modeline_identifier'"
  xrandr --addmode "$selected_output" "$custom_modeline_identifier"
  xrandr --output "$selected_output" --primary --mode "$custom_modeline_identifier"
fi

# Make sure gpu-screen-recorder is owned by nestri
sudo chown nestri:nestri /usr/bin/gpu-screen-recorder

if [[ -z "${SESSION_ID}" ]]; then
  echo "$(date +"[%Y-%m-%d %H:%M:%S]") No stream name was found, did you forget to set the env variable NAME?" && exit 1
else
  /usr/bin/gpu-screen-recorder -v no -w screen -c flv -f 60 -a "$(pactl get-default-sink).monitor" | ffmpeg -hide_banner -v quiet -i pipe:0 -c copy -f mp4 -movflags empty_moov+frag_every_frame+separate_moof+omit_tfhd_offset - | /usr/bin/warp --name "${SESSION_ID}" https://fst.so:4443 &
fi

openbox-session &

# /usr/games/gamescope -- mangohud glxgears > /dev/null & 

echo "$(date +"[%Y-%m-%d %H:%M:%S]") Session Running. Press [Return] to exit."
read
