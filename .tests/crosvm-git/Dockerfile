FROM archlinux:latest

RUN \
  pacman -Syu --noconfirm; \
	pacman -S --noconfirm \
  sudo \
  netctl \
  openvswitch \
  xfce4 \
  git \
  openssh \
  mesa-demos \
  htop \
  vulkan-tools \
  xorg-xwayland \
  wayland \
  pixman \
  libdrm \
  systemd \
  xcb-util \
  xcb-util-renderutil \
  xcb-util-wm \
  xcb-util-keysyms \  
  xcb-util-image \
  python \
  python-jinja \
  meson \
  ninja \
  base-devel

# Create the user and set the password
ARG USERVM=nestrivm
RUN useradd -m -G wheel -p '' $USERVM && passwd -d $USERVM

# Allow wheel group to use sudo without a password
RUN echo '%wheel ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers

# Set the hostname
RUN echo "crosvm-test" > /etc/hostname

# Configure SSH
RUN mkdir -p /home/$USERVM/.ssh && chown $USERVM:$USERVM /home/$USERVM/.ssh

# Set locales
ENV LANG="en_US.UTF-8"
ENV LANGUAGE="en_US:en"
ENV LC_ALL="en_US.UTF-8"
ARG TZ=UTC

RUN \
    echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && \
    locale-gen && \
    ln -snf "/usr/share/zoneinfo/${TZ}" /etc/localtime && echo "${TZ}" > /etc/timezone

# Clone the platform2 repository and build sommelier
RUN git clone https://chromium.googlesource.com/chromiumos/platform2

RUN \
    cd platform2/vm_tools/sommelier/ && \
    meson setup build -Dwith_tests=false && \
    meson compile -C build && \
    meson install -C build

