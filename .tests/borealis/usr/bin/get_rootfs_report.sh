#!/bin/bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

function md5_dir_digest() {
  local digest
  # Match md5sum format
  digest="$(tar -cf - --absolute-names "$1" | md5sum | awk '{print $1}')"
  echo "${digest}  ${1}"
}

md5_dir_digest "/etc/"       &
md5_dir_digest "/usr/lib32/" &
md5_dir_digest "/usr/lib64/" &
md5_dir_digest "/usr/share/" &

# Getting a directory digest of /usr/lib/ takes a very long time
# so get digests for relevant files only
graphics_regex='vk|vulkan|dri|gl|egl|wayland|x11|xcb|gstreamer'
find /usr/lib -type f | grep -v -E 'python|perl' | \
  grep -i -E "${graphics_regex}" | xargs md5sum

# /etc/ has been specifically problematic so get per-file digests too
# (b/254857730#comment2)
find /etc -type f -print0 | xargs -0 md5sum

wait
