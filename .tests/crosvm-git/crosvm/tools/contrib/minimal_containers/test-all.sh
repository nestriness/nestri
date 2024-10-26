# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tests if crosvm can be built and unit tests run for all containers defined in this directory.

set -e

cd $(dirname $0)

./run.sh Dockerfile.debian cargo build --workspace --features=all-x86_64
./run.sh Dockerfile.ubuntu cargo build --workspace --features=all-x86_64
