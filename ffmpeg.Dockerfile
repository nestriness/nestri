FROM ghcr.io/jrottenberg/ffmpeg:6.1-nvidia as build

FROM nvcr.io/nvidia/cuda:12.3.1-runtime-ubuntu22.04

# copy only needed files, without copying nvidia dev files
COPY --from=build /usr/local/bin /usr/local/bin/
COPY --from=build /usr/local/share /usr/local/share/
COPY --from=build /usr/local/lib /usr/local/lib/
COPY --from=build /usr/local/include /usr/local/include/

RUN ffmpeg -version