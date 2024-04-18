#!/bin/bash
#This fixes the error where x11-apps in docker cannot seem to access the hosts DISPLAY server
    # Error: No protocol specified
    # Error: Can't open display: :10.0
#https://askubuntu.com/a/1470341 //Run it on host OS
xhost +Local:*
xhost 

#Fun fact Weston won't run if you do not have Wayland running on the host (i have yet to try running with Xwayland inside the container)
#Run weston using your host's X11 display server
weston --backend=x11-backend.so

#Run inside the terminal of the weston you just created... cool right?
weston --backend=wayland-backend.so

#Run
docker run --gpus all --entrypoint /bin/bash --rm -it -v $(pwd):/games -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY --cap-add=SYS_NICE --cap-add=SYS_ADMIN recorder

/usr/bin/supervisord > /dev/null &

sudo apt install nvidia-driver-550 libnvidia-gl-550 nvidia-compute-utils-550
#This works
docker run -it --rm --gpus 1 --entrypoint /bin/bash --cap-add=SYS_NICE --cap-add=SYS_ADMIN  ghcr.io/wanjohiryan/netris/server:nightly
