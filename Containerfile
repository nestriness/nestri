
#******************************************************************************
#                                                                          base
#******************************************************************************
FROM archlinux:base-20241027.0.273886 AS base

# Update the pacman repo
RUN \
    pacman -Syu --noconfirm

#******************************************************************************
#                                                                       builder
#******************************************************************************

FROM base AS builder

RUN \
    pacman -Su --noconfirm \
    base-devel \
    git \
    sudo \
    vim

WORKDIR /scratch

# Allow nobody user to invoke pacman to install packages (as part of makepkg) and modify the system.
# This should never exist in a running image, just used by *-build Docker stages.
RUN \
  echo "nobody ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers;

ENV ARTIFACTS=/artifacts \
    CARGO_TARGET_DIR=/build

RUN \
  mkdir -p /artifacts \
  && mkdir -p /build
  
RUN \
  chgrp nobody /scratch /artifacts /build \
  && chmod g+ws /scratch /artifacts /build

#******************************************************************************
#                                                                  rust-builder
#******************************************************************************

FROM builder AS rust-builder

RUN \
    pacman -Su --noconfirm \
    rustup

RUN \
    rustup default stable

#******************************************************************************
#                                                         nestri-server-builder
#******************************************************************************
# Builds nestri server binary
FROM rust-builder AS nestri-server-builder

RUN \
    pacman -Su --noconfirm \
    wayland \
	vpl-gpu-rt \
	gstreamer \
	gst-plugin-va \
	gst-plugins-base \
	gst-plugins-good \
	mesa-utils \
	weston \
	xorg-xwayland


#******************************************************************************
#                                                          nestri-server-build
#******************************************************************************

FROM nestri-server-builder AS nestri-server-build

#Allow makepkg to be run as nobody.
RUN chgrp -R nobody /scratch && chmod -R g+ws /scratch

# USER nobody

# Perform the server build.
WORKDIR /scratch/server

RUN \
    git clone https://github.com/nestriness/nestri

WORKDIR /scratch/server/nestri

RUN \
    git checkout feat/stream \
    && cargo build -j$(nproc) --release

# COPY packages/server/build/ /scratch/server/

# RUN makepkg && cp *.zst "$ARTIFACTS"
#******************************************************************************
#                                                             runtime_base_pkgs
#******************************************************************************

FROM base AS runtime_base_pkgs

COPY --from=nestri-server-build /build/release/nestri-server /usr/bin/

#******************************************************************************
#                                                                  runtime_base
#******************************************************************************

FROM runtime_base_pkgs AS runtime_base

RUN \
    pacman -Su --noconfirm \
    weston \
    sudo \
    xorg-xwayland \
    gstreamer \
    gst-plugins-base \
    gst-plugins-good \
    gst-plugin-qsv \
    gst-plugin-va \
    gst-plugin-fmp4 \
    mesa \
    # Grab GPU encoding packages
    # Intel (modern VPL + VA-API)
    vpl-gpu-rt \
    intel-media-driver \
    # AMD/ATI (VA-API)
    libva-mesa-driver \
    # NVIDIA (proprietary)
    nvidia-utils

ENV \
    XDG_RUNTIME_DIR=/tmp