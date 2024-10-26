#!/bin/bash

# Copyright 2015 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

v=$1
include_dir=$2

sed \
  -e "s/@BSLOT@/${v}/g" \
  -e "s:@INCLUDE_DIR@:${include_dir}:g" \
  "libminijail.pc.in" > "libminijail.pc"
