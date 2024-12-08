#! /bin/bash -e

# sudo apt install build-essential -y

# To run tests, run the relay first - go run main.go.
# Run the docker container next - docker run --rm --init -d --device /dev/dri --network=host test-server 
# Then run the nestri-test-server - cd packages/relay/dev cargo run 
# Then run the frontend site, and navigate to http://localhost:5713/play/test

# Expected behavior, see some random messages on the browser's console tab
# And if you input works correctly, it should be logged to the console on the server-side of things

# docker build -t test-server -f Containerfile .

# docker run --rm --init -d --device /dev/dri --network=host test-server 

# echo -e "Navigate to http://localhost:5713/play/test"