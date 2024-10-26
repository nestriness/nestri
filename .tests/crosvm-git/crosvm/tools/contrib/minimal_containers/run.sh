# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Example usage:
#
#   ./run.sh Dockerfile.ubuntu cargo test --lib --bins --workspace

set -e

cd $(dirname $0)

CROSVM_ROOT=$(realpath "../../../")
FILENAME=$1
shift
DOCKER_BUILDKIT=1 docker build -t crosvm_minimal -f $FILENAME $CROSVM_ROOT

if [[ $# -eq 0 ]]; then
    docker run --rm -it --volume "${CROSVM_ROOT}:/workspace" crosvm_minimal
else
    docker run --rm -it --volume "${CROSVM_ROOT}:/workspace" crosvm_minimal bash -c "${*@Q}"
fi

