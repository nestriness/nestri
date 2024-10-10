# Docker to RootFS

We are building the rootfs as a docker image, mainly for consistency and ease of use.
So, to convert the Docker image to a rootfs good enough to run inside CrosVM we use this script

Run it like so:
```bash
./make your-docker-image
```

TODO:
1. Make sure the docker image name passed in exists
2. Reduce the dependencies of this script to 1 (If possible)
3. Extract not only the rootfs, but also kernel and initrd
4. Add a way to pass in the size of the rootfs the user wants
