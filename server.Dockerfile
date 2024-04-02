#This contains all the necessary libs for the server to work.
#NOTE: KEEP THIS IMAGE AS LEAN AS POSSIBLE.

FROM ubuntu:22.04

#FIXME: install https://git.dec05eba.com/gpu-screen-recorder (should be done in the .scripts/stream)
#TODO: Add mango-hud and gamescope
#FIXME: add pulseaudio

ENV DEBIAN_FRONTEND=noninteractive \
    TIMEZONE=Africa/Nairobi

#Install base libs
RUN apt update && \
    dpkg --add-architecture i386 && \
    apt install -y \
    software-properties-common \
    curl \
    apt-transport-https \
    apt-utils \
    wget \
    git \
    jq \
    locales \
    && rm -rf /var/lib/apt/lists/* \
    && locale-gen en_US.UTF-8

#Set language variables
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8

# Expose NVIDIA libraries and paths
ENV PATH=/usr/local/nvidia/bin:${PATH} \
    LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}:/usr/local/nvidia/lib:/usr/local/nvidia/lib64 \
    # Make all NVIDIA GPUs visible by default
    NVIDIA_VISIBLE_DEVICES=all \
    # All NVIDIA driver capabilities should preferably be used, check `NVIDIA_DRIVER_CAPABILITIES` inside the container if things do not work
    NVIDIA_DRIVER_CAPABILITIES=all \
    # Disable VSYNC for NVIDIA GPUs
    __GL_SYNC_TO_VBLANK=0

ENV XDG_RUNTIME_DIR=/run/user/1000/ \ 
    # DISPLAY=:0 \
    WAYLAND_DISPLAY=wayland-0 \
    PUID=0 \
    PGID=0 \
    UNAME="root"

RUN apt-get update -y \
    && apt-get install -y --no-install-recommends \
    libwayland-server0 \
    libwayland-client0 \
    xwayland \ 
    xdg-user-dirs \
    xdg-utils \
    #Vulkan
    mesa-vulkan-drivers \
    mesa-vulkan-drivers:i386 \
    libvulkan-dev \
    libvulkan-dev:i386 \
    vulkan-tools \
    # Install OpenGL libraries
    libglvnd0 \
    libglvnd0:i386 \
    libgl1 \
    libgl1:i386 \
    libglx0 \
    libglx0:i386 \
    libegl1 \
    libegl1:i386 \
    libgles2 \
    libgles2:i386 \
    libglu1 \
    libglu1:i386 \
    libsm6 \
    libsm6:i386 \
    && rm -rf /var/lib/apt/lists/* \
    && echo "/usr/local/nvidia/lib" >> /etc/ld.so.conf.d/nvidia.conf \
    && echo "/usr/local/nvidia/lib64" >> /etc/ld.so.conf.d/nvidia.conf \
    # Configure Vulkan manually
    && VULKAN_API_VERSION=$(dpkg -s libvulkan1 | grep -oP 'Version: [0-9|\.]+' | grep -oP '[0-9]+(\.[0-9]+)(\.[0-9]+)') \
    && mkdir -pm755 /etc/vulkan/icd.d/ \
    && echo "{\n\
        \"file_format_version\" : \"1.0.0\",\n\
        \"ICD\": {\n\
            \"library_path\": \"libGLX_nvidia.so.0\",\n\
            \"api_version\" : \"${VULKAN_API_VERSION}\"\n\
        }\n\
    }" > /etc/vulkan/icd.d/nvidia_icd.json \
    # Configure EGL manually
    && mkdir -pm755 /usr/share/glvnd/egl_vendor.d/ \
    && echo "{\n\
        \"file_format_version\" : \"1.0.0\",\n\
        \"ICD\": {\n\
            \"library_path\": \"libEGL_nvidia.so.0\"\n\
        }\n\
    }" > /usr/share/glvnd/egl_vendor.d/10_nvidia.json \
    # Prepare the XDG_RUNTIME_DIR for wayland
    && mkdir -p ${XDG_RUNTIME_DIR} && chmod 0700 ${XDG_RUNTIME_DIR}

#Install wine
# Wine, Winetricks, Lutris, and PlayOnLinux, this process must be consistent with https://wiki.winehq.org/Ubuntu
ARG WINE_BRANCH=staging
RUN mkdir -pm755 /etc/apt/keyrings && curl -fsSL -o /etc/apt/keyrings/winehq-archive.key "https://dl.winehq.org/wine-builds/winehq.key" \
    && curl -fsSL -o "/etc/apt/sources.list.d/winehq-$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"').sources" "https://dl.winehq.org/wine-builds/ubuntu/dists/$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"')/winehq-$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"').sources" \
    && apt-get update && apt-get install --install-recommends -y winehq-${WINE_BRANCH}

COPY .scripts/ /usr/bin/netris/

RUN ls -la /usr/bin/netris \
    && chmod +x /usr/bin/netris/proton

#Install proton
RUN /usr/bin/netris/proton -i

ENV TINI_VERSION v0.19.0
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /tini
RUN chmod +x /tini
ENTRYPOINT ["/tini", "--"]

CMD [ "/bin/bash" ]