# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Pushes the current repo to github using the secret provided by Secret Manager.
# See: https://console.cloud.google.com/security/secret-manager
#
# This script will only work if you have access to the above service via gcloud.
set -e
TOKEN=$(gcloud --project=crosvm-infra secrets versions access latest --secret="github-crosvm-bot")
git push --force "https://crosvm-bot:${TOKEN}@github.com/google/crosvm.git" HEAD:main
