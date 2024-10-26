#!/bin/bash
# Copyright 2017 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PV="1.14.0"

wget -q -nc --secure-protocol=TLSv1 \
  "https://github.com/google/googletest/archive/v${PV}.tar.gz" \
  -O "googletest-${PV}.tar.gz"
tar zxvf "googletest-${PV}.tar.gz"
