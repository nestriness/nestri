#! /bin/bash -e

docker build -t test-server -f Containerfile .

docker run --rm --init -d --device /dev/dri --network=host test-server 

echo -e "Navigate to http://localhost:5713/play/test"