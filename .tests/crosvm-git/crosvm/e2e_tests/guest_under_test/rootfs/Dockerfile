# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
ARG ARCH
FROM ${ARCH}/debian:bookworm

RUN --mount=type=cache,target=/var/cache/apt,sharing=private \
    --mount=type=cache,target=/var/lib/apt,sharing=private \
    apt-get update \
    && apt-get install --yes pciutils ncat alsa-utils sox sg3-utils

# Note: This docker file is run from the $(target)/rootfs directory specified in
#       the Makefile.

# Copy helper binaries
COPY ./delegate /bin/delegate
COPY ./readclock /bin/readclock
