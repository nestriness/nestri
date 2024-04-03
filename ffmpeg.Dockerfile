FROM ghcr.io/jrottenberg/ffmpeg:6.1-nvidia

RUN ffmpeg -v

# copy only needed files, without copying nvidia dev files
# COPY --from=build /usr/local/bin /usr/local/bin/
# COPY --from=build /usr/local/share /usr/local/share/
# COPY --from=build /usr/local/lib /usr/local/lib/
# COPY --from=build /usr/local/include /usr/local/include/