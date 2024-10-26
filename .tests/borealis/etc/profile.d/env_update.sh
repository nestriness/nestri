# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Append /opt/bin path if it exists at runtime.
if [ -d /opt/bin ]; then
  export PATH="${PATH}:/opt/bin"
fi
