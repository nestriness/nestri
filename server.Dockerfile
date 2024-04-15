#This contains all the necessary libs for the server to work.
#NOTE: KEEP THIS IMAGE AS LEAN AS POSSIBLE.
FROM ghcr.io/wanjohiryan/netris/base:nightly

ENV TZ=UTC \
    SIZEW=1920 \
    SIZEH=1080 \
    REFRESH=60 \
    DPI=96 \
    CDEPTH=24 \
    VIDEO_PORT=DFP

#Install Mangohud and gamescope
RUN apt-get update -y \
    && add-apt-repository -y multiverse \
    && apt-get install -y --no-install-recommends \
    mangohud \
    gamescope \
    && rm -rf /var/lib/apt/lists/*

#Install wine
ARG WINE_BRANCH=staging
RUN mkdir -pm755 /etc/apt/keyrings && curl -fsSL -o /etc/apt/keyrings/winehq-archive.key "https://dl.winehq.org/wine-builds/winehq.key" \
    && curl -fsSL -o "/etc/apt/sources.list.d/winehq-$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"').sources" "https://dl.winehq.org/wine-builds/ubuntu/dists/$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"')/winehq-$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"').sources" \
    && apt-get update && apt-get install --install-recommends -y winehq-${WINE_BRANCH}

#Install Proton
COPY .scripts/proton /usr/bin/netris/
RUN chmod +x /usr/bin/netris/proton \
    && /usr/bin/netris/proton -i

ARG USERNAME=ubuntu
# Create user and assign adequate groups
RUN apt-get update && apt-get install --no-install-recommends -y \
        sudo \
        tzdata \
    && rm -rf /var/lib/apt/lists/* \
    && usermod -a -G adm,audio,cdrom,dialout,dip,fax,floppy,input,lp,lpadmin,plugdev,pulse-access,render,scanner,ssl-cert,sudo,tape,tty,video,voice $USERNAME \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers \
    && chown $USERNAME:$USERNAME /home/$USERNAME \
    && ln -snf "/usr/share/zoneinfo/$TZ" /etc/localtime && echo "$TZ" > /etc/timezone

COPY warp /usr/bin/netris/
RUN chmod +x /usr/bin/netris/warp
COPY .scripts/entrypoint.sh .scripts/supervisord.conf /etc/
RUN chmod 755 /etc/supervisord.conf /etc/entrypoint.sh

USER 1000
ENV SHELL=/bin/bash \
    USER=${USERNAME}

WORKDIR /home/${USERNAME}

ENTRYPOINT ["/usr/bin/supervisord"]