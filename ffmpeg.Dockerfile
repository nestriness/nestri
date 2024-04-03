# From https://github.com/jrottenberg/ffmpeg/blob/main/docker-images/6.1/ubuntu2204/Dockerfile
# TODO: use nvenc? ghcr.io/jrottenberg/ffmpeg:6.1-ubuntu
FROM ghcr.io/jrottenberg/ffmpeg:6.1-nvidia as build

FROM ubuntu:22.04

ENV NVIDIA_DRIVER_CAPABILITIES compute,utility,video

RUN apt-get -yqq update \
        && apt-get install -yq --no-install-recommends ca-certificates expat libgomp1 libxcb-shape0-dev \
        && apt-get autoremove -y \
        && apt-get clean -y \
        #Install cuda v12
        && wget https://developer.download.nvidia.com/compute/cuda/repos/debian12/x86_64/cuda-keyring_1.1-1_all.deb \
        && dpkg -i cuda-keyring_1.1-1_all.deb \
        && add-apt-repository contrib \
        && apt-get update \
        && apt-get -y install cuda-toolkit-12-4 

# copy only needed files, without copying nvidia dev files
COPY --from=build /usr/local/bin /usr/local/bin/
COPY --from=build /usr/local/share /usr/local/share/
COPY --from=build /usr/local/lib /usr/local/lib/
COPY --from=build /usr/local/include /usr/local/include/

ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH

RUN ffmpeg -version