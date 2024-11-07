
#******************************************************************************
#                                                                          base
#******************************************************************************
FROM archlinux:base-20241027.0.273886 AS base
# How to run -  docker run -it --rm --device /dev/dri nestri /bin/bash - DO NOT forget the ports
# TODO: Migrate XDG_RUNTIME_DIR to /run/user/1000
# TODO: Add nestri-server to pulseaudio.conf
# TODO: Add our own entrypoint, with our very own zombie ripper 🧟🏾‍♀️
# FIXME: Add user root to `pulse-access` group as well :D
# TODO: Test the whole damn thing

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
    nvidia-utils \
    # Audio
    pulseaudio \
    # Supervisor
    supervisor
    
RUN \
    # Set up our non-root user $(nestri)
    groupadd -g 1000 nestri \
    && useradd -ms /bin/bash nestri -u 1000 -g 1000  \
    && passwd -d nestri \
    # Setup Pulseaudio
    && useradd -d /var/run/pulse -s /usr/bin/nologin -G audio pulse \
    && groupadd pulse-access \
    && usermod -aG audio,input,render,video,pulse-access nestri \
    && echo "nestri ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers \
    && echo "Users created" \
    # Create an empty machine-id file
    && touch /etc/machine-id

ENV \
    XDG_RUNTIME_DIR=/tmp

#******************************************************************************
#                                                                       runtime
#******************************************************************************

FROM runtime_base AS runtime
# Setup supervisor #
RUN <<-EOF
    echo -e "
        [supervisord]
        user=root
        nodaemon=true
        loglevel=info
        logfile=/tmp/supervisord.log
        pidfile=/tmp/supervisord.pid

        [program:dbus]
        user=root
        command=dbus-daemon --system --nofork
        logfile=/tmp/dbus.log
        pidfile=/tmp/dbus.pid
        stopsignal=INT
        autostart=true
        autorestart=true
        priority=1

        [program:pulseaudio]
        user=root
        command=pulseaudio --daemonize=no --system --disallow-module-loading --disallow-exit --exit-idle-time=-1
        logfile=/tmp/pulseaudio.log
        pidfile=/tmp/pulseaudio.pid
        stopsignal=INT
        autostart=true
        autorestart=true
        priority=10
    " | tee /etc/supervisord.conf
EOF

RUN \
    chown -R nestri:nestri /tmp /etc/supervisord.conf

ENV USER=nestri
USER 1000

CMD ["/usr/bin/supervisord", "-c", "/etc/supervisord.conf"]
# Debug - pactl list