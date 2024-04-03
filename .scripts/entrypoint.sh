#!/bin/bash

#Start pulseaudio
/usr/bin/pulseaudio --system --verbose --log-target=stderr --realtime=true --disallow-exit -F /etc/pulse/default.pa