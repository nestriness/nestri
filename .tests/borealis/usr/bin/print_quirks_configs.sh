#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Print out quirks configs for feedback reports.
# We parse them before printing to remove comments, and check correctness.

USER_QUIRKS_FILE="/home/chronos/.config/borealis.textproto"
SYSTEM_QUIRKS_FILE="/etc/quirks/default.textproto"

# Length limits apply to feedback report entries. Print hashes of each file,
# just in case the content is truncated and we're able to match the hash to a
# known config.
#
# Hashes are normally redacted from feedback reports. In this case it's not
# necessary to redact since we also print the actual content, length allowing.
# Prefixing it with arbitrary letters (except a-f) prevents redaction. See
# https://crsrc.org/o/src/platform/redaction_tool/redaction_tool.cc?q=::RedactHashes
echo -n XX
sha256sum "${USER_QUIRKS_FILE}"
echo -n XX
sha256sum "${SYSTEM_QUIRKS_FILE}"

# Also print the user quirks file's size and permissions.
ls -l "${USER_QUIRKS_FILE}"

# Print the actual quirks files. User first, since it's more likely novel.
# If the system quirks file is truncated, we still have the hash to work from.
bdt validate-quirks --print --config="${USER_QUIRKS_FILE}"
bdt validate-quirks --print --config="${SYSTEM_QUIRKS_FILE}"
