#This contains all the necessary libs for the server to work.
#NOTE: KEEP THIS IMAGE AS LEAN AS POSSIBLE.

FROM ubuntu:23.10

#FIXME: install Vulkan drivers (should be done in the .scripts/gpu)
#FIXME: install https://git.dec05eba.com/gpu-screen-recorder (should be done in the .scripts/stream)

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

WORKDIR /tmp
#Running gamescope flatpak run org.freedesktop.Platform.VulkanLayer.gamescope
#Install Mangohud, gamemode and gamescope
RUN apt-get update -y \
    && add-apt-repository -y multiverse \
    && apt-get install -y --no-install-recommends \
    flatpak \
    mangohud \
    gamescope
    #TODO: Add gamemode back in, when we get the time
    # meson \
    # libsystemd-dev \
    # pkg-config \
    # ninja-build \
    # git \
    # libdbus-1-dev \
    # libinih-dev \
    # build-essential \
    # && git clone https://github.com/FeralInteractive/gamemode.git && cd gamemode \
    # && git checkout 1.7 && chmod +x ./bootstrap.sh && ./bootstrap.sh \
    # && gamemode -t   \
    # && flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo \
    # gamescope
    # && com.valvesoftware.Steam.Utility.gamescope
    # Gamemode
    # && flatpak install -y com.valvesoftware.Steam.CompatibilityTool.Proton-GE

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