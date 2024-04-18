# This builds and updates the screen recorder we use on Netris
# From https://git.dec05eba.com/gpu-screen-recorder
FROM ubuntu:23.10

# ENV DEBIAN_FRONTEND=noninteractive \
#     TIMEZONE=Africa/Nairobi \
#     XDG_RUNTIME_DIR=/tmp/runtime-ubuntu \ 
#     DISPLAY=:0 \    
#     PULSE_SERVER=unix:/run/pulse/native
#     # WAYLAND_DISPLAY=wayland-0

# # Install fundamental packages
RUN apt-get clean \
    && apt-get update \
    && apt-get upgrade -y \
    && apt-get install --no-install-recommends -y \
    apt-transport-https \
    apt-utils \
    build-essential \
    ca-certificates \
    curl \
    gnupg \
    locales \
    make \
    software-properties-common \
    wget \
    && rm -rf /var/lib/apt/lists/* \
    && locale-gen en_US.UTF-8

# # Set locales
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 

# # Install operating system libraries or packages
# RUN dpkg --add-architecture i386 \
#     && apt-get update \
#     && apt-get install --no-install-recommends -y \
#     alsa-base \
#     alsa-utils \
#     cups-browsed \
#     cups-bsd \
#     cups-common \
#     cups-filters \
#     printer-driver-cups-pdf \
#     file \
#     bzip2 \
#     gzip \
#     xz-utils \
#     unar \
#     rar \
#     unrar \
#     zip \
#     unzip \
#     zstd \
#     gcc \
#     git \
#     jq \
#     python3 \
#     python3-cups \
#     python3-numpy \
#     ssl-cert \
#     nano \
#     vim \
#     htop \
#     fakeroot \
#     fonts-dejavu \
#     fonts-freefont-ttf \
#     fonts-hack \
#     fonts-liberation \
#     fonts-noto \
#     fonts-noto-cjk \
#     fonts-noto-cjk-extra \
#     fonts-noto-color-emoji \
#     fonts-noto-extra \
#     fonts-noto-ui-extra \
#     fonts-noto-hinted \
#     fonts-noto-mono \
#     fonts-noto-unhinted \
#     fonts-opensymbol \
#     fonts-symbola \
#     fonts-ubuntu \
#     lame \
#     less \
#     libavcodec-extra \
#     libpulse0 \
#     pulseaudio \
#     supervisor \
#     net-tools \
#     packagekit-tools \
#     pkg-config \
#     mesa-utils \
#     va-driver-all \
#     va-driver-all:i386 \
#     i965-va-driver-shaders \
#     i965-va-driver-shaders:i386 \
#     intel-media-va-driver-non-free \
#     intel-media-va-driver-non-free:i386 \
#     libva2 \
#     libva2:i386 \
#     vainfo \
#     vdpau-driver-all \
#     vdpau-driver-all:i386 \
#     vdpauinfo \
#     mesa-vulkan-drivers \
#     mesa-vulkan-drivers:i386 \
#     libvulkan-dev \
#     libvulkan-dev:i386 \
#     vulkan-tools \
#     ocl-icd-libopencl1 \
#     clinfo \
#     dbus-user-session \
#     dbus-x11 \
#     libdbus-c++-1-0v5 \
#     xkb-data \
#     xauth \
#     xbitmaps \
#     xdg-user-dirs \
#     xdg-utils \
#     xfonts-base \
#     xfonts-scalable \
#     xinit \
#     xsettingsd \
#     libxrandr-dev \
#     x11-xkb-utils \
#     x11-xserver-utils \
#     x11-utils \
#     x11-apps \
#     xserver-xorg-input-all \
#     xserver-xorg-input-wacom \
#     xserver-xorg-video-all \
#     xserver-xorg-video-intel \
#     xserver-xorg-video-qxl \
#     # Install OpenGL libraries
#     libxau6 \
#     libxau6:i386 \
#     libxdmcp6 \
#     libxdmcp6:i386 \
#     libxcb1 \
#     libxcb1:i386 \
#     libxext6 \
#     libxext6:i386 \
#     libx11-6 \
#     libx11-6:i386 \
#     libxv1 \
#     libxv1:i386 \
#     libxtst6 \
#     libxtst6:i386 \
#     libglvnd0 \
#     libglvnd0:i386 \
#     libgl1 \
#     libgl1:i386 \
#     libglx0 \
#     libglx0:i386 \
#     libegl1 \
#     libegl1:i386 \
#     libgles2 \
#     libgles2:i386 \
#     libglu1 \
#     libglu1:i386 \
#     libsm6 \
#     libsm6:i386 \
#     mesa-utils \
#     mesa-utils-extra \
#     xcvt \
#     && rm -rf /var/lib/apt/lists/* \
#     && echo "/usr/local/nvidia/lib" >> /etc/ld.so.conf.d/nvidia.conf \
#     && echo "/usr/local/nvidia/lib64" >> /etc/ld.so.conf.d/nvidia.conf \
#     # Configure OpenCL manually
#     && mkdir -pm755 /etc/OpenCL/vendors \
#     && echo "libnvidia-opencl.so.1" > /etc/OpenCL/vendors/nvidia.icd \
#     # Configure Vulkan manually
#     && VULKAN_API_VERSION=$(dpkg -s libvulkan1 | grep -oP 'Version: [0-9|\.]+' | grep -oP '[0-9]+(\.[0-9]+)(\.[0-9]+)') \
#     && mkdir -pm755 /etc/vulkan/icd.d/ \
#     && echo "{\n\
#         \"file_format_version\" : \"1.0.0\",\n\
#         \"ICD\": {\n\
#             \"library_path\": \"libGLX_nvidia.so.0\",\n\
#             \"api_version\" : \"${VULKAN_API_VERSION}\"\n\
#         }\n\
#     }" > /etc/vulkan/icd.d/nvidia_icd.json \
#     # Configure EGL manually
#     && mkdir -pm755 /usr/share/glvnd/egl_vendor.d/ \
#     && echo "{\n\
#         \"file_format_version\" : \"1.0.0\",\n\
#         \"ICD\": {\n\
#             \"library_path\": \"libEGL_nvidia.so.0\"\n\
#         }\n\
#     }" > /usr/share/glvnd/egl_vendor.d/10_nvidia.json

# Expose NVIDIA libraries and paths
ENV PATH=/usr/local/nvidia/bin${PATH:+:${PATH}} \
    LD_LIBRARY_PATH=${LD_LIBRARY_PATH:+${LD_LIBRARY_PATH}:}/usr/local/nvidia/lib:/usr/local/nvidia/lib64 \
    XDG_SESSION_TYPE=x11 \
    # Enable AppImage execution in containers
    APPIMAGE_EXTRACT_AND_RUN=1

ENV \
    # Make all NVIDIA GPUs visible by default
    NVIDIA_VISIBLE_DEVICES=all \
    # All NVIDIA driver capabilities should preferably be used, check `NVIDIA_DRIVER_CAPABILITIES` inside the container if things do not work
    NVIDIA_DRIVER_CAPABILITIES=all \
    # Disable VSYNC for NVIDIA GPUs
     __GL_SYNC_TO_VBLANK=0 

# #Build and install gpu-screen-recorder
# RUN apt-get update -y \
#     && apt-get install -y \
#     curl \
#     unzip \
#     git \
#     build-essential \
#     ninja-build \
#     gcc \
#     meson \
#     cmake \
#     ccache \
#     bison \
#     software-properties-common \
#     ca-certificates \
#     equivs \
#     ca-certificates\
#     libcap2-bin \
#     libllvm15 \
#     libavcodec-dev \
#     libavformat-dev \
#     libavutil-dev \
#     libavfilter-dev \
#     libavdevice-dev \
#     libswresample-dev \
#     libswscale-dev \
#     libx11-dev \
#     libxcomposite-dev \
#     libkpipewire-dev \
#     libxrandr-dev \
#     libxfixes-dev \
#     libpulse-dev \
#     libswresample-dev \
#     libva-dev \
#     libcap-dev \
#     libdrm-dev \
#     libgl-dev \
#     libegl-dev \
#     libwayland-dev \
#     libnvidia-egl-wayland-dev \
#     libwayland-egl-backend-dev \
#     wayland-protocols \
#     && rm -rf /var/lib/apt/lists/* \
#     #Install Nvrtc
#     && NVRTC_VERSION="11.0.221" \
#     && cd /tmp && curl -fsSL -o nvidia_cuda_nvrtc_linux_x86_64.whl "https://developer.download.nvidia.com/compute/redist/nvidia-cuda-nvrtc/nvidia_cuda_nvrtc-${NVRTC_VERSION}-cp36-cp36m-linux_x86_64.whl" \
#     && unzip -joq -d ./nvrtc nvidia_cuda_nvrtc_linux_x86_64.whl && cd nvrtc && chmod 755 libnvrtc* \
#     && find . -maxdepth 1 -type f -name "*libnvrtc.so.*" -exec sh -c 'ln -snf $(basename {}) libnvrtc.so' \; \
#     && mkdir -p /usr/local/nvidia/lib && mv -f libnvrtc* /usr/local/nvidia/lib \
#     && git clone https://repo.dec05eba.com/gpu-screen-recorder && cd gpu-screen-recorder \
#     && chmod +x ./build.sh ./install.sh \
#     && ./install.sh 

# #Try building shadow-cast
# RUN git clone https://github.com/gmbeard/shadow-cast && cd shadow-cast \
#     && mkdir ./build && cd ./build \ 
#     && cmake -DCMAKE_CXX_FLAGS="-Wno-error=unused-result" -DCMAKE_C_FLAGS="-Wno-error=unused-result" .. \ 
#     && cmake --build . -- -j$(nproc) \
#     && chmod +x ./install-helper.sh \
#     && ./install-helper.sh

# RUN apt-get update -y; \
#     apt-get upgrade -y; \
#     add-apt-repository ppa:savoury1/ffmpeg4 \
#     add-apt-repository ppa:savoury1/ffmpeg6 \
#     apt-get update -y; \
#     apt-get upgrade -y && apt-get dist-upgrade -y; \
#     apt-get install ffmpeg -y; \
#     #
#     # Log the ffmpeg version
#     ffmpeg -version    

# Install Xorg and NVIDIA driver installer dependencies
RUN dpkg --add-architecture i386 \
    && apt-get update \
    && apt-get install --no-install-recommends -y \
        kmod \
        libc6-dev \
        libc6:i386 \
        libpci3 \
        libelf-dev \
        pkg-config \
        xorg \
        && rm -rf /var/lib/apt/lists/*

#Install pipewire
RUN apt-get update \
    && apt-get install -y \
    pulseaudio \
    pipewire \
    pipewire-pulse \
    pipewire-audio-client-libraries \
    pipewire-media-session- \
    wireplumber \
    dbus-x11 \
    rtkit \
    && rm -rf /var/lib/apt/lists/* \
    && cp /usr/share/doc/pipewire/examples/alsa.conf.d/99-pipewire-default.conf /etc/alsa/conf.d/
