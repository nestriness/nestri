#!/bin/bash -e
NAME="${NAME:-$(head /dev/urandom | LC_ALL=C tr -dc 'a-zA-Z0-9' | head -c 16)}"

docker run --gpus all --device=/dev/dri --rm -it -e  SESSION_ID=$NAME -v "$(pwd)":/game --cap-add=SYS_NICE --cap-add=SYS_ADMIN server glxinfo #For testing