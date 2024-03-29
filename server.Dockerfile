#This contains all the necessary libs for the server to work.
#NOTE: KEEP THIS IMAGE AS LEAN AS POSSIBLE.

FROM ubuntu:latest

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

#Install proton
RUN /usr/bin/netris/proton -i