#!/bin/bash -e
trap "echo TRAPed signal" HUP INT QUIT TERM

# Create and modify permissions of XDG_RUNTIME_DIR
sudo -u netris mkdir -pm700 /tmp/runtime-1000
sudo chown netris:netris /tmp/runtime-1000
sudo -u netris chmod 700 /tmp/runtime-1000
# Make user directory owned by the user in case it is not
sudo chown netris:netris /home/netris || sudo chown netris:netris /home/netris/* || { echo "$(date +"[%Y-%m-%d %H:%M:%S]") Failed to change user directory permissions. There may be permission issues."; }

#Input devices ownable by our default user
export REQUIRED_DEVICES=${REQUIRED_DEVICES:-/dev/uinput /dev/input/event*}

declare -A group_map

for dev in $REQUIRED_DEVICES; do
  if [ -e "$dev" ]; then
    dev_group=$(stat -c "%G" "$dev")
    dev_gid=$(stat -c "%g" "$dev")

    if [ "$dev_group" = "UNKNOWN" ]; then
      new_name="netris-gid-$dev_gid"
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

#Install proton for this user, this will skip if the container is reused and the user has proto already installed
/usr/bin/proton -i

# Install NVIDIA userspace driver components including X graphic libraries
if ! command -v nvidia-xconfig &>/dev/null; then
  # Driver version is provided by the kernel through the container toolkit
  export DRIVER_ARCH="$(dpkg --print-architecture | sed -e 's/arm64/aarch64/' -e 's/armhf/32bit-ARM/' -e 's/i.*86/x86/' -e 's/amd64/x86_64/' -e 's/unknown/x86_64/')"
  export DRIVER_VERSION="$(head -n1 </proc/driver/nvidia/version | awk '{print $8}')"
  cd /tmp
  # If version is different, new installer will overwrite the existing components
  if [ ! -f "/tmp/NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}.run" ]; then
    # Check multiple sources in order to probe both consumer and datacenter driver versions
    curl -fsSL -O "https://international.download.nvidia.com/XFree86/Linux-${DRIVER_ARCH}/${DRIVER_VERSION}/NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}.run" || curl -fsSL -O "https://international.download.nvidia.com/tesla/${DRIVER_VERSION}/NVIDIA-Linux-${DRIVER_ARCH}-${DRIVER_VERSION}.run" || {
      echo "$(date +"[%Y-%m-%d %H:%M:%S]") Failed NVIDIA GPU driver download. Exiting."
      exit 1
    }
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

# Allow starting Xorg from a pseudoterminal instead of strictly on a tty console
if [ ! -f /etc/X11/Xwrapper.config ]; then
  echo -e "allowed_users=anybody\nneeds_root_rights=yes" | sudo tee /etc/X11/Xwrapper.config >/dev/null
fi
if grep -Fxq "allowed_users=console" /etc/X11/Xwrapper.config; then
  sudo sed -i "s/allowed_users=console/allowed_users=anybody/;$ a needs_root_rights=yes" /etc/X11/Xwrapper.config
fi

# Remove existing Xorg configuration
if [ -f "/etc/X11/xorg.conf" ]; then
  sudo rm -f "/etc/X11/xorg.conf"
fi

# Get first GPU device if all devices are available or `NVIDIA_VISIBLE_DEVICES` is not set
if [ "$NVIDIA_VISIBLE_DEVICES" == "all" ] || [ -z "$NVIDIA_VISIBLE_DEVICES" ]; then
  export GPU_SELECT="$(sudo nvidia-smi --query-gpu=uuid --format=csv | sed -n 2p)"
# Get first GPU device out of the visible devices in other situations
else
  export GPU_SELECT="$(sudo nvidia-smi --id=$(echo "$NVIDIA_VISIBLE_DEVICES" | cut -d ',' -f1) --query-gpu=uuid --format=csv | sed -n 2p)"
  if [ -z "$GPU_SELECT" ]; then
    export GPU_SELECT="$(sudo nvidia-smi --query-gpu=uuid --format=csv | sed -n 2p)"
  fi
fi

if [ -z "$GPU_SELECT" ]; then
  echo "$(date +"[%Y-%m-%d %H:%M:%S]") No NVIDIA GPUs detected or nvidia-container-toolkit not configured. Exiting."
  exit 1
fi

# Setting `VIDEO_PORT` to none disables RANDR/XRANDR, do not set this if using datacenter GPUs
if [ "${VIDEO_PORT,,}" = "none" ]; then
  export CONNECTED_MONITOR="--use-display-device=None"
# The X server is otherwise deliberately set to a specific video port despite not being plugged to enable RANDR/XRANDR, monitor will display the screen if plugged to the specific port
else
  export CONNECTED_MONITOR="--connected-monitor=${VIDEO_PORT}"
fi

# Bus ID from nvidia-smi is in hexadecimal format and should be converted to decimal format (including the domain) which Xorg understands, required because nvidia-xconfig doesn't work as intended in a container
HEX_ID="$(sudo nvidia-smi --query-gpu=pci.bus_id --id="$GPU_SELECT" --format=csv | sed -n 2p)"
IFS=":." ARR_ID=($HEX_ID)
unset IFS
BUS_ID="PCI:$((16#${ARR_ID[1]}))@$((16#${ARR_ID[0]})):$((16#${ARR_ID[2]})):$((16#${ARR_ID[3]}))"
# A custom modeline should be generated because there is no monitor to fetch this information normally
export MODELINE="$(cvt -r "${SIZEW}" "${SIZEH}" "${REFRESH}" | sed -n 2p)"
# Generate /etc/X11/xorg.conf with nvidia-xconfig
sudo nvidia-xconfig --virtual="${SIZEW}x${SIZEH}" --depth="$CDEPTH" --mode="$(echo "$MODELINE" | awk '{print $2}' | tr -d '\"')" --allow-empty-initial-configuration --no-probe-all-gpus --busid="$BUS_ID" --include-implicit-metamodes --mode-debug --no-sli --no-base-mosaic --only-one-x-screen ${CONNECTED_MONITOR}
# Guarantee that the X server starts without a monitor by adding more options to the configuration
sudo sed -i '/Driver\s\+"nvidia"/a\    Option         "ModeValidation" "NoMaxPClkCheck,NoEdidMaxPClkCheck,NoMaxSizeCheck,NoHorizSyncCheck,NoVertRefreshCheck,NoVirtualSizeCheck,NoExtendedGpuCapabilitiesCheck,NoTotalSizeCheck,NoDualLinkDVICheck,NoDisplayPortBandwidthCheck,AllowNon3DVisionModes,AllowNonHDMI3DModes,AllowNonEdidModes,NoEdidHDMI2Check,AllowDpInterlaced"' /etc/X11/xorg.conf
# Add custom generated modeline to the configuration
sudo sed -i '/Section\s\+"Monitor"/a\    '"$MODELINE" /etc/X11/xorg.conf
# Prevent interference between GPUs, add this to the host or other containers running Xorg as well
echo -e "Section \"ServerFlags\"\n    Option \"AutoAddGPU\" \"false\"\nEndSection" | sudo tee -a /etc/X11/xorg.conf >/dev/null

# Default display is :0 across the container
export DISPLAY=":0"
# Run Xorg server with required extensions
/usr/bin/Xorg vt7 -noreset -novtswitch -sharevts -dpi "${DPI}" +extension "COMPOSITE" +extension "DAMAGE" +extension "GLX" +extension "RANDR" +extension "RENDER" +extension "MIT-SHM" +extension "XFIXES" +extension "XTEST" "${DISPLAY}" &

# Wait for X11 to start
echo "$(date +"[%Y-%m-%d %H:%M:%S]") Waiting for X socket"
until [ -S "/tmp/.X11-unix/X${DISPLAY/:/}" ]; do sleep 1; done
echo "$(date +"[%Y-%m-%d %H:%M:%S]") X socket is ready"

if [[ -z "${NAME}" ]]; then
  echo "$(date +"[%Y-%m-%d %H:%M:%S]") No stream name was found, did you forget to set the env variable NAME?" && exit 1
else
  /usr/bin/gpu-screen-recorder -w screen -c flv -f 60 -a "$(pactl get-default-sink).monitor" | ffmpeg -hide_banner -v quiet -i pipe:0 -c copy -f mp4 -movflags empty_moov+frag_every_frame+separate_moof+omit_tfhd_offset - | /usr/bin/warp --name "${NAME}" https://fst.so:4443 &
fi

openbox-session &

#Now we can safely run our input server without permission errors
sudo /inputtino/input-server &

/usr/games/gamescope -- mangohud glxgears > /dev/null &

echo "$(date +"[%Y-%m-%d %H:%M:%S]") Session Running. Press [Return] to exit."
read
