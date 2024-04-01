#This contains all the necessary libs for the server to work.
#NOTE: KEEP THIS IMAGE AS LEAN AS POSSIBLE.

FROM ubuntu:22.04

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

COPY .scripts/ /usr/bin/netris/

RUN ls -la /usr/bin/netris \
    && chmod +x /usr/bin/netris/proton

# Expose NVIDIA libraries and paths
ENV PATH=/usr/local/nvidia/bin:${PATH} \
    LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/lib/i386-linux-gnu${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}:/usr/local/nvidia/lib:/usr/local/nvidia/lib64 \
    # Make all NVIDIA GPUs visible by default
    NVIDIA_VISIBLE_DEVICES=all \
    # All NVIDIA driver capabilities should preferably be used, check `NVIDIA_DRIVER_CAPABILITIES` inside the container if things do not work
    NVIDIA_DRIVER_CAPABILITIES=all \
    # Disable VSYNC for NVIDIA GPUs
    __GL_SYNC_TO_VBLANK=0

#Install proton
# RUN /usr/bin/netris/proton -i

ENV TINI_VERSION v0.19.0
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /tini
RUN chmod +x /tini
ENTRYPOINT ["/tini", "--"]

CMD [ "/bin/bash" ]