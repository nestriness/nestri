# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

FROM debian:bullseye
LABEL description="Test crosvm using a command like the following: \
docker run --privileged -v /dev/log:/dev/log -v <path to crosvm>:/platform/crosvm:ro <crosvm base image>"

# should be set to the ID/GROUP_ID of the user running the docker image
ARG USER_ID
ARG GROUP_ID

RUN apt-get update && \
    apt-get install -y --no-install-recommends ca-certificates gnupg && \
    echo 'deb-src https://deb.debian.org/debian bullseye main' >> /etc/apt/sources.list && \
    cat /etc/apt/sources.list && \
    apt-get update && \
    apt-get build-dep -y mesa && \
    apt-get install -y --no-install-recommends ca-certificates \
                                               python3-setuptools \
                                               llvm-dev \
                                               libxcb-shm0-dev \
                                               libelf-dev \
                                               cmake \
                                               chrony \
                                               bc \
                                               flex \
                                               bison \
                                               debootstrap \
                                               cpio \
                                               xz-utils \
                                               libegl1-mesa-dev \
                                               autoconf \
                                               automake \
                                               curl \
                                               g++ \
                                               gcc \
                                               gdb \
                                               git \
                                               kmod \
                                               libcap-dev \
                                               libdbus-1-dev \
                                               libegl1-mesa-dev \
                                               libepoxy-dev \
                                               libfdt-dev \
                                               libgl1-mesa-dev \
                                               libgles2-mesa-dev \
                                               libpciaccess-dev \
                                               libssl-dev \
                                               libtool \
                                               libusb-1.0-0-dev \
                                               libwayland-dev \
                                               make \
                                               nasm \
                                               ninja-build \
                                               pkg-config \
                                               protobuf-compiler \
                                               python \
                                               libtinfo5 \
                                               python3-protobuf \
                                               clang \
                                               iptables \
                                               libunwind-dev \
                                               libprotobuf-dev \
                                               protobuf-compiler \
                                               libprotoc-dev \
                                               libdw-dev \
                                               libprotobuf-dev \
                                               libdocopt-dev \
                                               && \
    apt-get -y build-dep intel-gpu-tools

ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:$PATH \
    RUST_VERSION=1.45.2 \
    RUSTFLAGS='--cfg hermetic'

# Debian usually has an old rust version in the repository. Instead of using that, we use rustup to
# pull in a toolchain versions of our choosing.
RUN curl -LO "https://static.rust-lang.org/rustup/archive/1.22.1/x86_64-unknown-linux-gnu/rustup-init" \
    && echo "49c96f3f74be82f4752b8bffcf81961dea5e6e94ce1ccba94435f12e871c3bdb *rustup-init" | sha256sum -c - \
    && chmod +x rustup-init \
    && ./rustup-init -y --no-modify-path --default-toolchain $RUST_VERSION \
    && rm rustup-init \
    && chmod -R a+w $RUSTUP_HOME $CARGO_HOME \
    && rustup --version \
    && cargo --version \
    && rustc --version

# Set the default toolchain to 'stable' to match the one that bin/smoke_test
# uses. This allows kokoro runs to avoid re-downloading the toolchain as long
# as the version matches RUST_VERSION.
RUN rustup default stable

# Warms up the cargo registry cache for future cargo runs. Cargo will still update the cache using a
# git pull, but it only needs to download files that were changed since this image was built.
RUN cargo install thisiznotarealpackage -q || true

# Used /scratch for building dependencies which are too new or don't exist on Debian stretch.
WORKDIR /scratch

# Suppress warnings about detached HEAD, which will happen a lot and is meaningless in this context.
RUN git config --global advice.detachedHead false

# New libepoxy and libdrm-dev requires newer meson than is in Debian stretch.
ARG MESON_COMMIT=master
RUN git clone https://github.com/mesonbuild/meson /meson \
    && cd /meson \
    && git checkout $MESON_COMMIT \
    && rm -f /usr/bin/meson \
    && ln -s $PWD/meson.py /usr/bin/meson

# The libdrm-dev in distro can be too old to build minigbm,
# so we build it from upstream.
ARG DRM_COMMIT=master
RUN git clone https://gitlab.freedesktop.org/mesa/drm.git/ \
    && cd drm \
    && git checkout $DRM_COMMIT \
    && meson build -Dlibdir=lib \
    && ninja -C build/ install

# The gbm used by upstream linux distros is not compatible with crosvm, which must use Chrome OS's
# minigbm.
RUN git clone https://chromium.googlesource.com/chromiumos/platform/minigbm \
    && cd minigbm \
    && sed 's/-Wall/-Wno-maybe-uninitialized/g' -i Makefile \
    && make CPPFLAGS="-DDRV_I915" DRV_I915=1 install -j$(nproc)

RUN git clone https://gitlab.freedesktop.org/virgl/virglrenderer.git \
    && cd virglrenderer \
    && mkdir -p build \
    && meson build/ -Dprefix=/usr/local -Dlibdir=lib \
    && ninja -C build/ install

# Install libtpm2 so that tpm2-sys/build.rs does not try to build it in place in
# the read-only source directory.
ARG TPM2_COMMIT=073dc25aa4dda42475a7a5a140399fc5db61b20f
RUN git clone https://chromium.googlesource.com/chromiumos/third_party/tpm2 \
    && cd tpm2 \
    && git checkout $TPM2_COMMIT \
    && make -j$(nproc) \
    && cp build/libtpm2.a /lib

# PUll down platform2 repositroy and install librendernodehost.
# Note that we clone the repository outside of /scratch not to be removed
# because crosvm depends on libvda.
ENV PLATFORM2_ROOT=/platform2
ARG PLATFORM2_COMMIT=2dce812fc9091e41a33094929610199468ee322b
RUN git clone https://chromium.googlesource.com/chromiumos/platform2 $PLATFORM2_ROOT \
    && cd $PLATFORM2_ROOT \
    && git checkout $PLATFORM2_COMMIT

# Set up sysroot from which system_api proto files are built.
ENV SYSROOT=/sysroot
RUN mkdir -p $SYSROOT/usr/include/chromeos/dbus/trunks \
    && cp $PLATFORM2_ROOT/trunks/interface.proto \
        $SYSROOT/usr/include/chromeos/dbus/trunks
# Copy it under rustc's sysroot as well for cargo clippy.
RUN export RUST_SYSROOT=$(rustc --print sysroot); echo $RUST_SYSROOT
RUN mkdir -p $RUST_SYSROOT/usr/include/chromeos/dbus/trunks \
  && cp $PLATFORM2_ROOT/trunks/interface.proto \
        $RUST_SYSROOT/usr/include/chromeos/dbus/trunks

# Reduces image size and prevents accidentally using /scratch files
RUN rm -r /scratch
WORKDIR /

# The manual installation of shared objects requires an ld.so.cache refresh.
RUN ldconfig

# Pull down repositories that crosvm depends on to cros checkout-like locations.
ENV CROS_ROOT=/
ENV THIRD_PARTY_ROOT=$CROS_ROOT/third_party
RUN mkdir -p $THIRD_PARTY_ROOT
ENV PLATFORM_ROOT=$CROS_ROOT/platform
RUN mkdir -p $PLATFORM_ROOT
ENV AOSP_EXTERNAL_ROOT=$CROS_ROOT/aosp/external
RUN mkdir -p $AOSP_EXTERNAL_ROOT

# minijail does not exist in upstream linux distros.
ARG MINIJAIL_COMMIT=5f9e3001c61626d2863dad91248ba8496c3ef511
RUN git clone https://android.googlesource.com/platform/external/minijail $AOSP_EXTERNAL_ROOT/minijail \
    && cd $AOSP_EXTERNAL_ROOT/minijail \
    && git checkout $MINIJAIL_COMMIT \
    && make -j$(nproc) \
    && cp libminijail.so /usr/lib/x86_64-linux-gnu/

# Pull the cras library for audio access.
ARG ADHD_COMMIT=5068bdd18b51de8f2d5bcff754cdecda80de8f44
RUN git clone https://chromium.googlesource.com/chromiumos/third_party/adhd $THIRD_PARTY_ROOT/adhd \
    && cd $THIRD_PARTY_ROOT/adhd \
    && git checkout $ADHD_COMMIT

ARG VPERFETTO_COMMIT=3ce4813ae114e5f2e6e0b3f29517a88246c00363
RUN git clone https://github.com/741g/vperfetto.git && \
  cd vperfetto && \
  git checkout $VPERFETTO_COMMIT && \
  cmake -G Ninja -B_build -DOPTION_BUILD_TESTS=FALSE && \
  ninja -C _build install

ARG CROSVM_COMMIT=3f9373f474a295df0f8a38592472ae59adc98e29
RUN mkdir -p /platform/ \
   && cd /platform \
   && git clone --single-branch -b perfetto https://gitlab.freedesktop.org/tomeu/crosvm.git \
   && cd crosvm \
   && cargo install --locked --debug --features 'default-no-sandbox wl-dmabuf gpu x virtio-gpu-next' --path . --root /usr/local

RUN export uid=$USER_ID gid=$GROUP_ID && \
    mkdir -p /home/chronos && \
    echo "chronos:x:${uid}:${gid}:Developer,,,:/home/chronos:/bin/bash" >> /etc/passwd && \
    echo "chronos:x:${uid}:" >> /etc/group && \
    chown ${uid}:${gid} -R /home/chronos


#crony didn't install cleanly when using --include, so we add it by using an extra apt install

ENV EXTRA_PACKAGES="sudo,strace,libxcb-dri2-0,libxcb-dri3-0,libx11-xcb1,libxcb-xfixes0,libxcb-present0,libxcb-sync1,libxshmfence1,libx11-6,sysvinit-core,libwayland-client0,libwayland-server0,time,inetutils-ping,dnsutils,libpng16-16,libprocps8,valgrind,libsensors5,python3,curl,gnupg,ca-certificates,llvm-dev,chrony"

RUN ulimit -n 1024 && \
    debootstrap --variant=minbase --components main,contrib,non-free --include=$EXTRA_PACKAGES  bullseye /rootfs http://deb.debian.org/debian && \
    chroot /rootfs /bin/bash -c "apt install -yy gdb" && \
    chroot /rootfs /bin/bash -c "dpkg-query -Wf '\${Installed-Size}\t\${Package}\n' | sort -n " && \
    chroot /rootfs /bin/bash -c "useradd -u 1001 -r -d / -s /sbin/nologin -c 'crossvm image user' perfetto" 

COPY perf-testing/Docker/init.sh /rootfs/.

RUN cd /rootfs && \
  find -H | cpio -H newc -o | xz --check=crc32 -T4 - > /rootfs.cpio.gz

COPY perf-testing/Docker/x86_64.config /tmp/.
RUN mkdir -p kernel && \
  curl -L --retry 4 -f --retry-all-errors --retry-delay 60 \
     https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.10.16.tar.xz -o - | \
     tar -xJ --strip-components=1 -C kernel && \
  cd kernel && \
  ./scripts/kconfig/merge_config.sh arch/x86/configs/x86_64_defconfig /tmp/x86_64.config && \
  make -j12 vmlinux && \
  cp vmlinux /. && \
  cd .. && \
  rm -rf kernel

# Need an unreleased version of Waffle for surfaceless support in apitrace
# Replace this build with the Debian package once that's possible
ENV WAFFLE_VERSION="b6d94483694b6889a8567b6bd748f7baad527df4"
RUN git clone https://gitlab.freedesktop.org/mesa/waffle.git --single-branch --no-checkout /waffle && \
  cd /waffle && \
  git checkout "$WAFFLE_VERSION" && \
  cmake -B_build -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_BUILD_TYPE=Debug -Dwaffle_has_surfaceless_egl=1 . && \
  make -j12 -C _build install && \
  mkdir -p build/lib build/bin && \
  cp _build/lib/libwaffle-1.so build/lib/libwaffle-1.so.0 && \
  cp _build/bin/wflinfo build/bin/wflinfo

ENV APITRACE_VERSION="perfetto"
RUN git clone https://gitlab.freedesktop.org/tomeu/apitrace.git --single-branch -b perfetto --no-checkout /apitrace && \
  cd /apitrace && \
  git checkout "$APITRACE_VERSION" && \
  cmake -G Ninja -B_build -H. -DCMAKE_BUILD_TYPE=Debug -DENABLE_GUI=False -DENABLE_WAFFLE=on -DWaffle_DIR=/usr/local/lib/cmake/Waffle/ && \
  ninja -C _build && \
  mkdir build && \
  cp _build/apitrace build && \
  cp _build/eglretrace build

ENV GN_ARGS="is_debug=false use_custom_libcxx=false"
ENV CFG=linux_trusty-gcc7-x86_64-release
RUN git clone https://android.googlesource.com/platform/external/perfetto && \
  cd perfetto && \
  git checkout v12.1 && \
  python3 tools/install-build-deps && \
  python3 tools/install-build-deps --ui && \
  tools/gn gen out/dist --args="${GN_ARGS}" --check && \
  tools/ninja -C out/dist traced traced_probes perfetto trace_to_text ui trace_processor_shell && \
  mkdir -p /usr/local/lib/python3.7/site-packages && \
  protoc --python_out=/usr/local/lib/python3.7/site-packages protos/perfetto/trace/perfetto_trace.proto && \
  tools/gen_amalgamated --gn_args 'target_os="linux" is_debug=false'

RUN mkdir -p /traces-db && chown chronos:chronos /traces-db && mkdir -p /wd && chown -R chronos:chronos /wd

ENV IGT_GPU_TOOLS_VERSION="igt-gpu-tools-1.25"
RUN git clone --single-branch -b master https://gitlab.freedesktop.org/drm/igt-gpu-tools.git && \
    cd igt-gpu-tools && \
    git checkout "$IGT_GPU_TOOLS_VERSION" && \
    meson build -Doverlay=disabled -Dchamelium=disabled -Dvalgrind=disabled -Dman=disabled -Ddocs=disabled -Dtests=disabled -Drunner=disabled && \
    ninja -C build install

ENV GFX_PPS_VERSION="v0.3.0"
RUN git clone --single-branch -b master https://gitlab.freedesktop.org/Fahien/gfx-pps.git && \
    cd gfx-pps && \
    git checkout "$GFX_PPS_VERSION" && \
    meson build -Dtest=false -Dbuildtype=debugoptimized && \
    ninja -C build

COPY perf-testing/Docker/run_traces.sh /usr/local/.
COPY perf-testing/Docker/run_perfetto_ui.sh /usr/local/.
COPY perf-testing/Docker/run.sh /usr/local/.
COPY perf-testing/Docker/perfetto-guest.cfg /usr/local/.
COPY perf-testing/Docker/perfetto-host.cfg /usr/local/.
COPY perf-testing/Docker/merge_traces.py /usr/local/.

ENTRYPOINT ["/usr/local/run.sh"]
