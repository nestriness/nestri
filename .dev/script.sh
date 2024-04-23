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

docker run --gpus all --entrypoint /bin/bash --device=/dev/dri --rm -it -v $(pwd):/game --cap-add=SYS_NICE --cap-add=SYS_ADMIN ghcr.io/wanjohiryan/netris/server:nightly

ffmpeg -hide_banner -v quiet -stream_loop -1 -re -i /game/test.mp4 -an -f mp4 -movflags empty_moov+frag_every_frame+separate_moof+omit_tfhd_offset - | RUST_LOG=moq_pub=info warp --name "netris" https://fst.so:4443

docker run -d --gpus all -e NAME=netris --device=/dev/dri --rm -v $(pwd):/game -p 8080:8080 -v /dev/input:/dev/input:rw --device /dev/uinput --cap-add=SYS_NICE --cap-add=SYS_ADMIN server
 
/usr/games/gamescope -- mangohud /usr/bin/netris/proton -r /game/AlanWake2.exe