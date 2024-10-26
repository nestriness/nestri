#!/bin/bash
# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
version=$1
libdir=$2
cat <<END
Name: perfetto
Description: Perfetto SDK
Version: ${version}
Libs: -L/usr/${libdir} -lperfetto -pthread
Cflags: -I/usr/include
END
