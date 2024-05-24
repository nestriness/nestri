#!/bin/bash -e
NAME="${NAME:-$(head /dev/urandom | LC_ALL=C tr -dc 'a-zA-Z0-9' | head -c 16)}"

# docker run --gpus all --device=/dev/dri --rm -it --entrypoint /bin/bash -e  SESSION_ID=G4r06Kc8vDmwIXPG -v "$(pwd)":/game -p 8080:8080/udp --cap-add=SYS_NICE --cap-add=SYS_ADMIN server
docker run --gpus all --device=/dev/dri --rm -it --entrypoint /bin/bash -e  SESSION_ID=$NAME$NAME -v "$(pwd)":/game -p 8080:8080/udp --cap-add=SYS_NICE --cap-add=SYS_ADMIN server