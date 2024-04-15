#!/bin/bash

# Attempt to set capabilities
# Gotten from https://git.dec05eba.com/gpu-screen-recorder/tree/install.sh
#TODO: remove this
# setcap 'cap_sys_admin+ep' /usr/bin/gsr-kms-server
# setcap 'cap_sys_nice+ep' /usr/bin/gpu-screen-recorder

#Start dbus
/etc/init.d/dbus start

#Start Pulseaudio Reference: https://github.com/wanjohiryan/warp/blob/ad9cd38d21f0ac4332e64358e219b48e01871870/docker/nvidia/entrypoint.sh#L38
/usr/bin/pulseaudio -k >/dev/null 2>&1 || /usr/bin/pulseaudio --system --verbose --log-target=stderr -D --realtime=true --disallow-exit -L 'module-native-protocol-tcp auth-ip-acl=127.0.0.0/8 port=4713 auth-anonymous=1'
pacmd load-module module-virtual-sink sink_name=vsink
pacmd set-default-sink vsink
pacmd set-default-source vsink.monitor

# Create and modify permissions of XDG_RUNTIME_DIR Reference: https://github.com/selkies-project/docker-nvidia-glx-desktop/blob/94b139c5d04395e1171202bb41e5d6f60e576a39/entrypoint.sh#L9C1-L10C1
mkdir -pm700 /tmp/runtime-user
chown root:root /tmp/runtime-user
chmod 700 /tmp/runtime-user

#FIXME:

#Mangohud [Works]
# Mangohud errors: Selected GPU 0: Tesla T4, type: DiscreteGpu [2024-04-14 23:52:57.098] [MANGOHUD] [error] [loader_nvctrl.cpp:39] Failed to open 64bit libXNVCtrl.so.0: libXNVCtrl.so.0: cannot open shared object file: No such file or directory [2024-04-14 23:52:57.098] [MANGOHUD] [error] [nvctrl.cpp:45] XNVCtrl loader failed to load 
# gpu-screen-recorder [Fails]: #/usr/games/gamescope -w 1920 -h 1080 -W 3440 -H 1440 -r 60 -f -F fsr -- mangohud vkcubeNo CAP_SYS_NICE, falling back to regular-priority compute and threads.Performance will be affected.wlserver: [backend/headless/backend.c:68] Creating headless backendvulkan: selecting physical device 'Tesla T4': queue family 2vulkan: physical device does not support DRM format modifiersvulkan: physical device has no render nodeFailed to initialize Vulkan
# TODO: replace gpu-screen-recorder with shadow-cast (has better documentation)