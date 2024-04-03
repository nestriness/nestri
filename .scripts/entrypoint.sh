#!/bin/bash

#Start pulseaudio
/usr/bin/pulseaudio --system --verbose --log-target=stderr --realtime=true --disallow-exit -F /etc/pulse/default.pa

# Attempt to set capabilities
# Gotten from https://git.dec05eba.com/gpu-screen-recorder/tree/install.sh
setcap 'cap_sys_admin+ep' /usr/bin/gsr-kms-server
setcap 'cap_sys_nice+ep' /usr/bin/gpu-screen-recorder