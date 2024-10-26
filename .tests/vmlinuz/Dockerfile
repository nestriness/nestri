FROM archlinux:latest AS base

RUN \
  pacman -Syu --noconfirm

###########################################################
FROM base AS builder

RUN pacman -Su --noconfirm \
  fakeroot \
  base-devel \
  git \
  sudo \
  vim

WORKDIR /scratch
RUN \
  echo "nobody ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers;

ENV ARTIFACTS=/artifacts
RUN \
  mkdir -p /artifacts

RUN \
  chgrp nobody /scratch /artifacts && \
  chmod g+ws /scratch /artifacts
########################################################
FROM builder AS kernel-builder

RUN pacman -Su --noconfirm \
  bc \
  cpio \
  gettext \
  libelf \
  pahole \
  perl \
  python \
  tar \
  xz \
  rsync
######################################################
FROM kernel-builder AS kernel-build

COPY kernel-git/ /scratch/kernel-git/

WORKDIR /scratch/kernel-git

# Allow makepkg to be run as nobody.
RUN chgrp -R nobody . && chmod -R g+ws .

USER nobody

RUN makepkg && cp *.zst "$ARTIFACTS"

########################################
FROM base AS runtime
RUN pacman -Su --noconfirm \
    sudo

WORKDIR /scratch

COPY --from=kernel-build /artifacts/*.zst pkgs/

RUN pacman -U --noconfirm pkgs/*.zst && rm -r pkgs

