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

#Install Mangohud, openbox and gamescope
RUN apt-get update -y \
    && add-apt-repository -y multiverse \
    && apt-get install -y --no-install-recommends \
    libxnvctrl0 \
    libevdev2 \
    mangohud \
    gamescope \
    openbox \
    && setcap cap_sys_nice+ep /usr/games/gamescope \
    && rm -rf /var/lib/apt/lists/*

#Install wine
ARG WINE_BRANCH=staging
RUN mkdir -pm755 /etc/apt/keyrings && curl -fsSL -o /etc/apt/keyrings/winehq-archive.key "https://dl.winehq.org/wine-builds/winehq.key" \
    && curl -fsSL -o "/etc/apt/sources.list.d/winehq-$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"').sources" "https://dl.winehq.org/wine-builds/ubuntu/dists/$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"')/winehq-$(grep UBUNTU_CODENAME= /etc/os-release | cut -d= -f2 | tr -d '\"').sources" \
    && apt-get update && apt-get install --install-recommends -y winehq-${WINE_BRANCH} \
    && curl -fsSL -o /usr/bin/winetricks "https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks" \
    && chmod 755 /usr/bin/winetricks \
    && curl -fsSL -o /usr/share/bash-completion/completions/winetricks "https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks.bash-completion"

#Install Proton
COPY .scripts/proton /usr/bin/netris/
RUN chmod +x /usr/bin/netris/proton \
    && /usr/bin/netris/proton -i

ARG USERNAME=netris \
    PUID=1000 \
    PGID=1000 \
    UMASK=000 \
    HOME="/home/netris"

ENV XDG_RUNTIME_DIR=/tmp/runtime-1000

# Create user and assign adequate groups
RUN apt-get update && apt-get install --no-install-recommends -y \
        sudo \
        tzdata \
    && rm -rf /var/lib/apt/lists/* \ 
    # Delete default user
    && if id -u "${PUID}" &>/dev/null; then \
      oldname=$(id -nu "${PUID}"); \
      if [ -z "${oldname}" ]; then \
        echo "User with UID ${PUID} exists but username could not be determined."; \
        exit 1; \
      else \
        userdel -r "${oldname}"; \
      fi \
    fi \
    # Now create ours
    && groupadd -f -g "${PGID}" ${USERNAME} \
    && useradd -m -d ${HOME} -u "${PUID}" -g "${PGID}" -s /bin/bash ${USERNAME} \
    && umask "${UMASK}" \
    && chown "${PUID}:${PGID}" "${HOME}" \
    && usermod -a -G adm,audio,cdrom,dialout,dip,fax,floppy,input,lp,lpadmin,plugdev,pulse-access,render,ssl-cert,sudo,tape,tty,video,voice $USERNAME \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers \
    && ln -snf "/usr/share/zoneinfo/$TZ" /etc/localtime && echo "$TZ" > /etc/timezone

COPY --from=ghcr.io/wanjohiryan/netris/warp:nightly /usr/bin/warp /usr/bin/
COPY --from=ghcr.io/wanjohiryan/netris/warp-input:nightly /usr/bin/warp-input /usr/bin/warp-input
RUN chmod +x /usr/bin/warp /usr/bin/warp-input
COPY .scripts /etc/
RUN chmod 755 /etc/supervisord.conf /etc/entrypoint.sh /etc/startup.sh

USER 1000
ENV SHELL=/bin/bash \
    USER=${USERNAME}
#For mounting the game into the container
VOLUME [ "/game" ]
#For inputtino server
EXPOSE 8080

WORKDIR /home/${USERNAME}

# ENTRYPOINT ["/usr/bin/supervisord"]
ENTRYPOINT ["/etc/startup.sh"]