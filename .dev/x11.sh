#! /bin/bash

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

#Start docker and run a game
docker run -it -v /tmp/.X11-unix:/tmp/.X11-unix -v $(pwd)/games:/games:ro --gpus all --entrypoint /bin/bash  -e DISPLAY=$DISPLAY server

#Run the game using proton
dbg=2 /usr/bin/netris/proton -r /games/npp.exe

#Sometimes the Xserver unix socket will disappear (and i have to reinitialiase the VM again), so here is the manual way to check
ls -la /tmp/.X11-unix #If something exists (anything for that matter :), then you are okay