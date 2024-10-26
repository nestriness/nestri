#!/bin/bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

# Syntax: stamp-lsb-release.sh [--prod] stage

LSB_RELEASE="/etc/lsb-release"
OS_RELEASE="/etc/os-release"

PROD_CLEANUP=0
if [[ "$1" == "--prod" ]]; then
  PROD_CLEANUP=1
  shift
fi

STAGE="$1"

# Delete existing lines in /etc/lsb-release
sed -i '/^NESTRI_\(STAGE\|DATE\)=/d' "${LSB_RELEASE}"

# Move default OS signatures to VM_OS_* and add in Nestri signatures
MV_PREPEND="VM_OS_"
sed -i "s/^\(NAME\)=/${MV_PREPEND}\1=/g" "${OS_RELEASE}"
sed -i "s/^\(PRETTY_NAME\)=/${MV_PREPEND}\1=/g" "${OS_RELEASE}"
sed -i "s/^\(DISTRIB_ID\)=/${MV_PREPEND}\1=/g" "${LSB_RELEASE}"
sed -i "s/^\(DISTRIB_DESCRIPTION\)=/${MV_PREPEND}\1=/g" "${LSB_RELEASE}"

echo "NAME=\"NestriOS (Arch Linux)\"" >> "${OS_RELEASE}"
echo "PRETTY_NAME=\"NestriOS (Arch Linux)\"" >> "${OS_RELEASE}"
echo "DISTRIB_ID=\"NestriOS (Arch Linux)\"" >> "${LSB_RELEASE}"
echo "DISTRIB_DESCRIPTION=\"NestriOS (Arch Linux)\"" \
    >> "${LSB_RELEASE}"

# Add our own
echo "Stamping NESTRI_STAGE=${STAGE} in ${LSB_RELEASE}"
echo "NESTRI_STAGE=${STAGE}" >> "${LSB_RELEASE}"
echo "NESTRI_DATE=\"$(date -I)\"" >> "${LSB_RELEASE}"

# Perform prod clean if requested
if grep 'NESTRI_DEBUG=1' "${LSB_RELEASE}"; then
  echo "Debug build; not cleaning up development files."
elif [[ "${PROD_CLEANUP}" == "1" ]]; then
  echo "Preserving licenses"
  # /usr/share/doc contains licenses referenced via symlinks from
  # /usr/share/licenses for some packages.  Save these to allow license
  # extraction to work.
  mkdir -p /usr/share/doc.save
  mv /usr/share/doc/{libjpeg-turbo,systemd,xz} /usr/share/doc.save
  rm -rf /usr/share/doc
  mv /usr/share/doc.save /usr/share/doc

  echo "Cleaning up development files and pacman cache for prod build"
  rm -rf /usr/include
  find /usr -name 'lib*.a' -delete
  # Specifying --clean twice cleans all packages, even installed ones. We pipe
  # in `yes` because specifying --noconfirm answers "no" to all the questions.
  yes | pacman --sync --clean --clean
fi

# Regenerate ldconfig cache at the end to include all installed shared libraries
echo "Updating ldconfig cache"
ldconfig
