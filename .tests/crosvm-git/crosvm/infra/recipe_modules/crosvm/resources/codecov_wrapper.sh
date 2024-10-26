# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Calls the provided command with CODECOV_TOKEN set. The token is obtained via the gcloud Secret
# Manager.
# See: https://console.cloud.google.com/security/secret-manager
#
# This script will only work if you have access to the above service via gcloud.
set -e
CODECOV_TOKEN=$(gcloud --project=crosvm-infra secrets versions access latest --secret="codecov-google-crosvm") $@
