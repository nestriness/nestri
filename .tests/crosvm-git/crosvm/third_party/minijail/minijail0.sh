#!/bin/sh
# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Helper for running minijail0 in a compiled checkout.

dir="$(dirname "$0")"
exec "${dir}/minijail0" --preload-library="${dir}/libminijailpreload.so" "$@"
