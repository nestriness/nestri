# From https://github.com/jrottenberg/ffmpeg/blob/main/docker-images/6.1/ubuntu2204/Dockerfile
# TODO: use nvenc? ghcr.io/jrottenberg/ffmpeg:6.1-ubuntu
FROM ghcr.io/jrottenberg/ffmpeg:6.1-ubuntu as build

FROM ubuntu:22.04

RUN apt-get -yqq update \
        && apt-get install -yq --no-install-recommends ca-certificates expat libgomp1 \
        && apt-get autoremove -y \
        && apt-get clean -y

# copy only needed files, without copying nvidia dev files
COPY --from=build /usr/local /usr/local/

ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64

RUN ffmpeg -version