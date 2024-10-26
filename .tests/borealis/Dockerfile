# This has been extracted and copied from ChromiumOs/Borealis
# Check out the license at https://chromium.googlesource.com/chromiumos/platform/borealis/+/refs/heads/main/LICENSE
# Why? They have figured out how to run games inside CrosVM, we tried vkcube won't run (sommelier and glx works tho)

FROM archlinux:base-20240101.0.204074 AS initial

# Most of /etc is added later on (to avoid invalidating the entire Docker
# cache too frequently), but we need pacman.conf ASAP.
COPY etc/pacman.conf* /etc/
COPY etc/pacman.d/* /etc/pacman.d/
# Symlinks don't properly copy into the container so manually create symlink
RUN \
  ln -sf /etc/pacman.d/siglevel.default /etc/pacman.d/siglevel
# Chain together these tasks to avoid Docker cache consistency issues
RUN \
  chmod a+r /etc/pacman.conf* \
              /etc/pacman.d/* \
  # Force update our databases using our historical mirror URL.
  && pacman -Syy --config=/etc/pacman.conf.db \
  # Update the Archlinux keyring to get new package maintainer keys
  && pacman -S --noconfirm archlinux-keyring \
  && pacman-key --init && pacman-key --populate archlinux \
  # Perform a full system upgrade using our package mirror.
  && pacman -Su --noconfirm \
      arch-audit \
      diffutils

#******************************************************************************
#                                                                       builder
#******************************************************************************

# builder has tooling to do basic 32-bit and 64-bit builds.
FROM initial AS builder
RUN \
  pacman -S --noconfirm \
      base-devel \
      clang \
      cmake \
      git \
      llvm \
      meson \
      multilib-devel \
      ninja \
      python3 \
      sudo \
      vim

WORKDIR /scratch

# Allow nobody user to invoke pacman to install packages as part of makepkg.
# This should never exist in a running image, just used by *-build Docker stages.
RUN \
  echo "nobody ALL=(ALL) NOPASSWD: /usr/sbin/pacman" >> /etc/sudoers; 

ENV ARTIFACTS=/artifacts
RUN \
  mkdir -p /artifacts

RUN \
  chgrp nobody /scratch /artifacts && \
  chmod g+ws /scratch /artifacts


#******************************************************************************
#                                                              perfetto-builder
#******************************************************************************
# perfetto-builder has libraries to build perfetto daemons.
FROM builder AS perfetto-builder
WORKDIR /scratch
# checkout a pinned Perfetto release available as git tag vNNN.N
RUN git clone --depth 1 --branch v33.0 \
    https://android.googlesource.com/platform/external/perfetto
WORKDIR /scratch/perfetto
RUN tools/write_version_header.py --stdout > /artifacts/version && \
    tools/install-build-deps


#******************************************************************************
#                                                                perfetto-build
#******************************************************************************
# perfetto-build builds perfetto daemons.
# TODO(davidriley): Convert this into a proper package.
FROM perfetto-builder AS perfetto-build
WORKDIR /scratch/perfetto
RUN tools/gn gen out/release \
        --args=" \
            is_clang=true \
            is_debug=false \
            enable_perfetto_zlib=false" \
# TODO(b/241286650) Re-add use_custom_libcxx=false to save rootfs space.
    && tools/ninja -C out/release tracebox \
    && mkdir -p /artifacts && cp out/release/stripped/tracebox /artifacts/


#******************************************************************************
#                                                            perfetto-sdk-build
#******************************************************************************
# perfetto-sdk-build builds the perfetto sdk.
FROM perfetto-builder AS perfetto-sdk-build
WORKDIR /scratch/perfetto
RUN CFLAGS='-fdata-sections -ffunction-sections' meson /scratch/builddir \
    && ninja -C /scratch/builddir \
    && cp sdk/perfetto.h /artifacts/ \
    && cp /scratch/builddir/libperfetto.a /artifacts/
COPY pkgconfig/perfetto.pc.sh /scratch/
RUN version=$(cat /artifacts/version) \
    && bash /scratch/perfetto.pc.sh ${version:1} lib64 > /artifacts/perfetto.pc


#******************************************************************************
#                                                      lib32-perfetto-sdk-build
#******************************************************************************
# lib32-perfetto-sdk-build builds the lib32 perfetto sdk.
FROM perfetto-builder AS lib32-perfetto-sdk-build
WORKDIR /scratch/perfetto
RUN CXXFLAGS='-m32' CFLAGS='-fdata-sections -ffunction-sections -m32' \
    meson /scratch/builddir \
    && ninja -C /scratch/builddir \
    && cp sdk/perfetto.h /artifacts/ \
    && cp /scratch/builddir/libperfetto.a /artifacts/
COPY pkgconfig/perfetto.pc.sh /scratch/
RUN version=$(cat /artifacts/version) \
    && bash /scratch/perfetto.pc.sh ${version:1} lib32 > /artifacts/perfetto.pc

#******************************************************************************
#                                                               wayland-builder
#******************************************************************************
# wayland-builder has prerequisites to build wayland
FROM builder AS wayland-builder
RUN \
  pacman -Su --noconfirm \
    docbook-xsl \
    graphviz

#******************************************************************************
#                                                                 wayland-build
#******************************************************************************
FROM wayland-builder AS wayland-build
COPY packages/cros-wayland-git/ /scratch/cros-wayland-git/
WORKDIR /scratch/cros-wayland-git

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . && chmod -R g+ws .

# Perform the build as nobody.
USER nobody
RUN \
  makepkg && cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                         lib32-wayland-builder
#******************************************************************************
# lib32-wayland-builder has prerequisites to build lib32-wayland
FROM builder AS lib32-wayland-builder
COPY --from=wayland-build /artifacts/*.zst pkgs/
RUN \
  pacman -U --noconfirm pkgs/*.zst \
  && pacman -S --noconfirm \
      docbook-xsl \
      graphviz \
      lib32-expat \
      lib32-glibc \
      lib32-libffi \
      lib32-libxml2


#******************************************************************************
#                                                           lib32-wayland-build
#******************************************************************************
FROM lib32-wayland-builder AS lib32-wayland-build
COPY packages/lib32-cros-wayland-git/ /scratch/lib32-cros-wayland-git/
WORKDIR /scratch/lib32-cros-wayland-git

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . && chmod -R g+ws .

# Perform the build as nobody.
USER nobody
RUN \
  makepkg && cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                                   gfx-builder
#******************************************************************************
# gfx-builder has prerequisites to build the graphics stack.
FROM builder AS gfx-builder
# Install packages already built.
WORKDIR /scratch
COPY --from=lib32-wayland-build /artifacts/*.zst pkgs/ 
COPY --from=wayland-build /artifacts/*.zst pkgs/

RUN \
  pacman -U --noconfirm pkgs/*.zst \
  && pacman -S --noconfirm \
        elfutils \
        glslang \
        libclc \
        libdrm \
        libunwind \
        libx11 \
        libxdamage \
        libxrandr \
        libxshmfence \
        libxxf86vm \
        lm_sensors \
        python-mako \
        python-packaging \
        python-ply \
        vulkan-icd-loader \
        vulkan-headers \
        vulkan-utility-libraries \
        wayland-protocols \
        xorgproto

#******************************************************************************
#                                                                  mesa-builder
#******************************************************************************
# mesa-builder has prerequisites to build mesa.
FROM gfx-builder AS mesa-builder
RUN \
  pacman -S --noconfirm \
      libglvnd \
      ocl-icd \
      spirv-llvm-translator


COPY --from=perfetto-sdk-build /artifacts/perfetto.h /usr/include/
COPY --from=perfetto-sdk-build /artifacts/libperfetto.a /usr/lib64/
COPY --from=perfetto-sdk-build /artifacts/perfetto.pc /usr/lib64/pkgconfig/

#******************************************************************************
#                                                                    mesa-build
#******************************************************************************
# mesa-build builds cros-mesa-git package.
FROM mesa-builder AS mesa-build

# Set to 1 to build native drivers, for borealis_chroot.
ARG BUILD_NATIVE_DRIVERS=0

# Sync package.
COPY packages/cros-mesa-git/ /scratch/cros-mesa-git/
WORKDIR /scratch/cros-mesa-git

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . && chmod -R g+ws .

# Perform the build as nobody.
USER nobody
RUN \
  env BUILD_NATIVE_DRIVERS="${BUILD_NATIVE_DRIVERS}" makepkg

RUN \
  cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                            lib32-mesa-builder
#******************************************************************************
# lib32-mesa-builder has prerequisites to build lib32-mesa.
FROM gfx-builder AS lib32-mesa-builder

# Install packages already built.
WORKDIR /scratch
COPY --from=lib32-wayland-build /artifacts/*.zst pkgs/
COPY --from=mesa-build /artifacts/*.zst pkgs/

RUN \
  pacman -U --noconfirm pkgs/*.zst \
  && pacman -S --noconfirm \
        glslang \
        lib32-clang \
        lib32-libdrm \
        lib32-libelf \
        lib32-libglvnd \
        lib32-libunwind \
        lib32-libx11 \
        lib32-libxdamage \
        lib32-libxml2 \
        lib32-libxrandr \
        lib32-libxshmfence \
        lib32-libxxf86vm \
        lib32-llvm \
        lib32-llvm-libs \
        lib32-lm_sensors \
        lib32-spirv-llvm-translator \
        lib32-vulkan-icd-loader \
        lib32-zstd \
        wayland-protocols


COPY --from=lib32-perfetto-sdk-build /artifacts/perfetto.h /usr/include/
COPY --from=lib32-perfetto-sdk-build /artifacts/libperfetto.a /usr/lib32/
COPY --from=lib32-perfetto-sdk-build \
     /artifacts/perfetto.pc /usr/lib32/pkgconfig/

#******************************************************************************
#                                                              lib32-mesa-build
#******************************************************************************
# lib32-mesa-build builds 32-bit lib32-cros-mesa-git package.
FROM lib32-mesa-builder AS lib32-mesa-build

# Set to 1 to build native drivers, for borealis_chroot.
ARG BUILD_NATIVE_DRIVERS=0

# Sync package.
COPY packages/lib32-cros-mesa-git/ /scratch/lib32-cros-mesa-git/
WORKDIR /scratch/lib32-cros-mesa-git

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . && chmod -R g+ws .

# Perform the build as nobody.
USER nobody
RUN \
  env BUILD_NATIVE_DRIVERS="${BUILD_NATIVE_DRIVERS}" makepkg
RUN \
  cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                              xwayland-builder
#******************************************************************************
# xwayland-builder has prerequisites to build xwayland.
FROM gfx-builder AS xwayland-builder

# Install packages already built.
WORKDIR /scratch
COPY --from=mesa-build /artifacts/*.zst pkgs/
RUN \
  pacman -U --noconfirm pkgs/*.zst \
  && pacman -S --noconfirm \
        egl-wayland \
        libepoxy \
        libxcvt \
        libxfont2 \
        libxkbfile \
        pixman \
        xorg-font-util \
        xorg-server-common \
        xtrans

# One of our xwayland patches needs virtwl.h.
# Putting it in /scratch/xwayland/include/linux causes the source package to
# fail to untar correctly, so install it system-wide instead. This should be
# fine since this container is only used to build this single package.
COPY include/linux/virtwl.h /usr/include/linux/


#******************************************************************************
#                                                                xwayland-build
#******************************************************************************
# xwayland-build builds cros-xwayland package.
FROM xwayland-builder AS xwayland-build

# Sync package.
COPY packages/cros-xwayland/ /scratch/cros-xwayland/
WORKDIR /scratch/cros-xwayland

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . \
    && chmod -R g+ws . \
    && chmod a+r /usr/include/linux/virtwl.h

# Perform the build as nobody.
# TODO(davidriley): Avoid --skippgpcheck.
USER nobody
RUN \
  makepkg --skippgpcheck

RUN \
  cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                             sommelier-builder
#******************************************************************************
# sommelier-builder has prerequisites to build sommelier.
FROM builder AS sommelier-builder

# Install packages already built.
WORKDIR /scratch
COPY  --from=mesa-build /artifacts/*.zst pkgs/ 
COPY  --from=wayland-build /artifacts/*.zst pkgs/

RUN \
  pacman -U --noconfirm pkgs/*.zst \
  && pacman -S --noconfirm \
        gtest \
        libevdev \
        libxkbcommon \
        pixman \
        protobuf \
        python-jinja

COPY --from=perfetto-sdk-build /artifacts/perfetto.h /usr/include/
COPY --from=perfetto-sdk-build /artifacts/libperfetto.a /usr/lib64/
COPY --from=perfetto-sdk-build /artifacts/perfetto.pc /usr/lib64/pkgconfig/

#******************************************************************************
#                                                               sommelier-build
#******************************************************************************
# sommelier-build builds sommelier package.
FROM sommelier-builder AS sommelier-build

# Sync package.
COPY packages/sommelier-git/ /scratch/sommelier-git/
WORKDIR /scratch/sommelier-git
#TODO: make sure you 
#TODO:  1. git clone https://chromium.googlesource.com/chromiumos/platform2
#TODO:  2. mv platform2/vm_tools/sommelier ./src (Move sommelier to the build directory)
#TODO:  3. get sommelier's version and add it to ./src/VERSION
#FIXME: Maybe create a bash script to do this? ASAP

COPY sommelier/ ./src/sommelier 
COPY stamp/sommelier ./src/VERSION

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . && chmod -R g+ws . && chown -R nobody ./src/

# Perform the build as nobody.
USER nobody
RUN \
  makepkg --noextract
RUN \
  cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                             audiotest-builder
#******************************************************************************
# audiotest-builder has prerequisites to build audiotest.
FROM builder AS audiotest-builder
WORKDIR /scratch
RUN pacman -S --noconfirm \
    alsa-lib

#******************************************************************************
#                                                               audiotest-build
#******************************************************************************
# audiotest-build builds loopback-latency and copies cyclic bench script
FROM audiotest-builder AS audiotest-build
COPY packages/audiotest-git /scratch/audiotest-git
WORKDIR /scratch/audiotest-git
# Allow makepkg to be run as nobody.
RUN chgrp -R nobody . && chmod -R g+ws .
# Perform the build as nobody
USER nobody
RUN makepkg && cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                                steamcmd-build
#******************************************************************************
FROM builder AS steamcmd-build
COPY packages/steamcmd /scratch/steamcmd
WORKDIR /scratch/steamcmd

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . && chmod -R g+ws .

# Perform the build as nobody.
USER nobody
RUN \
  makepkg
RUN \
  cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                                     vdf-build
#******************************************************************************
FROM builder AS vdf-build
# Build dependencies needed by vdf, not required for release.
RUN pacman -S --noconfirm \
    python-coverage \
    python-nose
COPY packages/vdf-git /scratch/vdf-git
WORKDIR /scratch/vdf-git
RUN chgrp -R nobody . && chmod -R g+ws .
# Perform the build as nobody.
USER nobody
RUN makepkg
RUN cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                                weston-builder
#******************************************************************************
# weston-builder has prerequisites to build weston
FROM gfx-builder AS weston-builder

COPY --from=mesa-build /artifacts/*.zst pkgs/ 
COPY --from=xwayland-build /artifacts/*.zst pkgs/

RUN \
  pacman -U --noconfirm pkgs/*.zst \
  && pacman -S --noconfirm \
        cairo \
        colord \
        lcms2 \
        libinput \
        libjpeg-turbo \
        libva \
        libwebp \
        libxcursor \
        libxkbcommon \
        mtdev \
        pango \
        pipewire \
        seatd \
        xcb-util-cursor


#******************************************************************************
#                                                                  weston-build
#******************************************************************************
# weston-build builds weston and its test clients
FROM weston-builder AS weston-build
COPY packages/weston /scratch/weston
WORKDIR /scratch/weston

# Allow makepkg to be run as nobody.
RUN \
  chgrp -R nobody . && chmod -R g+ws .

# perform the build as nobody
USER nobody
RUN \
  makepkg && cp *.zst "$ARTIFACTS"

#******************************************************************************
#                                                             runtime_base_pkgs
#******************************************************************************
# runtime_base_pkgs contains all the base packages copied into /scratch/pkgs.
FROM initial AS runtime_base_pkgs
WORKDIR /scratch

COPY --from=wayland-build /artifacts/*.zst pkgs/ 
COPY --from=lib32-wayland-build /artifacts/*.zst pkgs/ 
COPY --from=mesa-build /artifacts/*.zst pkgs/ 
COPY --from=lib32-mesa-build /artifacts/*.zst pkgs/ 
COPY --from=xwayland-build /artifacts/*.zst pkgs/
COPY --from=audiotest-build /artifacts/*.zst pkgs/
COPY --from=vdf-build /artifacts/*.zst pkgs/
COPY --from=weston-build /artifacts/*.zst pkgs/

#******************************************************************************
#                                                                  runtime_base
#******************************************************************************
# The runtime base image has all the 3rd party software installed which
# we need for the borealis runtime. We separate the base runtime from
# the "true" runtime image so that the later parts of the build can be
# done hermetically
FROM runtime_base_pkgs AS runtime_base
WORKDIR /scratch
RUN \
  rm pkgs/*-debug-*.zst \
  && pacman -U --noconfirm pkgs/*.zst \
  && rm -r pkgs

# xinput, evtest, joystick, xtrace
#  - Included for debugging purposes, not needed for correct operation.
#    TODO(cpelling): Consider removing xtrace/others to reduce image size.
# strace, lsof
#  - Included for tracing the X server, not needed for correct operation.

# Game-specific dependencies:
# - alsa-plugins - Don't Starve Together and games from Klei Entertainment
#   (pulled in along with lib32-alsa-plugins by steam)
# - pipewire-alsa - Don't Starve Together and games from Klei Entertainment
# - at-spi2-atk - Europa Universalis IV (pulled in by gtk3)
# - glu - Stoneshard, and likely other GameMaker Studio games
# - gtk3 - Europa Universalis IV
# - lib32-pipewire - Don't Starve, and games that use FMOD
# - libbsd - Dead Cells
# - libibus - Metro Exodus (pulled in by sdl2)
# - libnghttp2 - Loop Hero, Hero Siege
# - libjpeg-turbo - Needs to be reinstalled to populate /usr/share/doc
# - libxss - Dead Cells
# - libvorbis - Dead Cells
# - libxcrypt-compat - Loop Hero
# - openal - audio for Stoneshard, and likely other GameMaker Studio games
# - pulseaudio - Proton, and games that use PulseAudio
# - rtmpdump - Loop Hero (librtmp.so.1)
# - speex - Master of Orion 2 (Dosbox)
# - xorg-xrandr - Slay the Spire, LWJGL (Lightweight Java Game Library)
#   (pulled in by steam)
# - xz - Needs to be reinstalled to populate /usr/share/doc

# Test-specific dependencies:
# - fmt - GLBench and VKBench string formatting.
# - gflags - GLBench and VKBench argument parsing tool.
# - numactl - Dependency for cyclictest from rt-tests.
# - rt-tests - Real-time test utilities.
# - sox - Sound processing utilities.

# Other dependencies:
# - rsync - required for shader cache migration from the guest to host virtiofs
#           TODO(b/282080100): deprecate this dependency

## Remove some unnecessary packages
##  - packagekit is actually expected by garcon, but in borealis we want that
##    expectation to fail quickly rather than succeed/fail slowly.
#RUN apt-get -y remove \
#        packagekit \
#    && apt-get -y autoremove
RUN \
  pacman -Su --noconfirm --disable-download-timeout \
    acpid \
    alsa-utils \
    btrfs-progs \
    dbus \
    evtest \
    fmt \
    gflags \
    glfw-x11 \
    glu \
    gtk3 \
    joystick \
    lib32-pipewire \
    libbsd \
    libevdev \
    libjpeg-turbo \
    libnghttp2 \
    libvorbis \
    libxcb \
    libxcrypt-compat \
    lib32-libxcrypt-compat \
    libxss \
    lsof \
    mesa-utils \
    numactl \
    openal \
    pciutils \
    pulseaudio \
    pipewire \
    pipewire-alsa \
# For running X11 app fakes.
    python-xlib \
    rsync \
    rtkit \
    rtmpdump \
    rt-tests \
    sdl2 \
# TODO(b/325093294): Remove socat once missing game logs is fixed in the
# latest Proton stable version.
    socat \
    sox \
    speex \
    strace \
    ttf-liberation \
# For vulkaninfo and vkcube
    vulkan-tools \
    wget \
    which \
    wireplumber \
    xdg-utils \
    xorg-xdpyinfo \
    xorg-xdriinfo \
    xorg-xev \
    xorg-xinput \
    xorg-xkbutils \
# For trace_window_system.sh
    xorg-xlsatoms \
# For xwindump.py, used in feedback reports
    xorg-xwininfo \
    xz \
# For file choosing
    xdg-desktop-portal \
    xdg-desktop-portal-gtk \
# For kernel and initrd
    linux

# b/315759185: Delete command line tool frequently implicated in CVEs.
RUN \
  rm -fv /usr/sbin/gif2rgb /usr/bin/gif2rgb

# TODO(b/333416855): Temporarily work around incompatible Steam package
# with -chromeos option.
ARG STEAM_PKG=steam-1.0.0.78-2-x86_64.pkg.tar.zst
RUN pacman --noconfirm -U \
   "https://storage.googleapis.com/chromeos-mirror/archlinux/multilib/os/x86_64/$STEAM_PKG"

# Copy over the binaries and scripts for traces
#COPY win_tools/bin/exerun.py /opt/win_tools/bin/
#COPY win_tools/bin/trace_capture.py /opt/win_tools/bin/

# Install first-party packages last, since they change often and won't be
# pulled in as dependencies of third-party packages. This allows us to cache
# more of the previous steps when iterating locally.
COPY --from=sommelier-build /artifacts/*.zst pkgs/
RUN \
  rm pkgs/*-debug-*.zst \
  && pacman -U --noconfirm pkgs/*.zst \
  && rm -r pkgs


#******************************************************************************
#                                                                       runtime
#******************************************************************************
# The runtime image is the actual borealis product
FROM runtime_base AS runtime


# Create users and groups for various services.
#
# Note: When started from the UI, Steam and games run under the set of groups
# specified in etc/init.d/cros-garcon. Be sure to update that file when
# adding chronos to a new group.
#
RUN \
  groupadd -g 1000 nestri \
  && useradd -ms /bin/bash nestri -u 1000 -g 1000  \
    && passwd -d nestri \
    && usermod -a -G audio,input,render,video,nestri \
        nestri \
    && echo "Users created" \
# Create an empty machine-id file
    && touch /etc/machine-id \
# /mnt/empty: for jailing the logging daemon
# /mnt/stateful: writable home dir
# /mnt/external: for mounting external drives
    && mkdir -vp \
         /mnt/empty \
         /mnt/external \
         /mnt/shared \
         /mnt/stateful \
#Generate the locale
    && echo "LC_ALL=en_US.UTF-8" >> /etc/environment \
    && echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen \
    && echo "LANG=en_US.UTF-8" > /etc/locale.conf \
    && locale-gen en_US.UTF-8 \
    && echo "Locale set and generated"

# Set up /etc
COPY etc/ /etc/

# Set up custom scripts
COPY usr/bin/ /usr/bin/

# Steam does not support chained-symlinks
# (https://github.com/ValveSoftware/steam-for-linux/issues/5609).
# Therefore, we bind-mount zoneinfo files onto /etc/localtime in runtime in
# maitred. In order to bind-mount, we need a starting file.
# TODO(b/237960004): Clean this up after Steam supports chained symlinks.
RUN \
  cp /usr/share/zoneinfo/UTC /etc/localtime

#******************************************************************************
#                                                                  runtime_prod
#******************************************************************************
# The production image merely adds some metadata, and cleans up development
# headers and static libraries, and the package cache.
# FIXME: Protobuf is always missing in production, why?
FROM runtime AS runtime_prod
RUN \
  /usr/bin/stamp-lsb-release.sh --prod runtime_prod
  && pacman -Su --noconfirm \
      protobuf \
      sudo \
  && echo "nestri ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers \
  && echo "root:root" | chpasswd
