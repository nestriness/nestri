# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Development container for crosvm.
#
# Provides all dependencies specified in install-deps with some additonal
# logic to cache cargo data in CI runs.

# Build catapult dashboard upload tool in a builder container
FROM docker.io/golang:bullseye AS gobuilder
WORKDIR /root/
RUN git clone https://fuchsia.googlesource.com/infra/infra
WORKDIR /root/infra/cmd/catapult
RUN go build

FROM docker.io/debian:testing-slim

ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/workspace/tools:/usr/local/cargo/bin:$PATH

# Install pipx applications globally in /usr/local/bin
ENV PIPX_HOME=/usr/local/pipx \
    PIPX_BIN_DIR=/usr/local/bin

# Use a dedicated target directory so we do not write into the source directory.
RUN mkdir -p /scratch/cargo_target \
    && mkdir /cache

# Prevent the container from writing __pycache__ files into the src.
ENV PYTHONDONTWRITEBYTECODE=1
ENV CARGO_TARGET_DIR=/scratch/cargo_target

# Allow APT to cache packages between docker image builds
RUN rm -f /etc/apt/apt.conf.d/docker-clean; echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache

# Add foreign architectures for cross-compilation.
RUN dpkg --add-architecture arm64 \
    && dpkg --add-architecture armhf

# Install dependencies (APT and cargo packages are cached between image builds for faster iterative builds).
COPY --chmod=555 tools/install-deps tools/install-aarch64-deps tools/install-armhf-deps tools/install-mingw64-deps tools/setup-wine64 rust-toolchain /tools/
RUN cd /tools \
    && apt-get update \
    && apt-get install --yes sudo curl \
    && ./install-deps \
    # We only use the nightly toolchain for rustfmt, remove other parts to save space.
    && rustup component remove --toolchain nightly rust-std \
    && rustup component remove --toolchain nightly cargo

# HACK ALERT!
# libc6:arm64 postinstall will attempt to call telinit to restart services. This will block
# indefinitely in the container with no init running. Replace telinit with true to make it
# a no-op. See b/322015733
RUN rm /usr/sbin/telinit && cp /bin/true /usr/sbin/telinit

RUN cd /tools \
    && ./install-aarch64-deps \
    && ./install-armhf-deps \
    && ./install-mingw64-deps

# Add wine64 to PATH, as debian removed alternative entry to wine64
ENV PATH=/usr/lib/wine:$PATH

# Setup wine for root user
RUN /tools/setup-wine64

# Install global config.toml for cross-compilation
COPY --chmod=555 .cargo/config.debian.toml /.cargo/config.toml

# Install catapult dashboard upload tool
COPY --from=gobuilder /root/infra/cmd/catapult/catapult /tools/

# Cache CARGO_HOME between container runs in CI.
VOLUME /cache
ENV CROSVM_CACHE_DIR=/cache
ENV CARGO_HOME=/cache/cargo_home

VOLUME /workspace
WORKDIR /workspace
