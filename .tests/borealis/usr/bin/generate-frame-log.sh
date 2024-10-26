#!/bin/sh
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

count="$(find /tmp -name "timing-trace*" | wc -l)"
pkill sommelier --newest --exact --signal USR1
until [ "$(find /tmp -name "timing-trace*" | wc -l)" -gt "${count}" ]
    do sleep 0.1
done
filename="$(find /tmp -name "timing-trace*" | sort -V | tail -1)"
alias ttrace='grep -q -E "^EndTime [0-9]+ [0-9]+\.[0-9]+.*" "${filename}"'
until ttrace
    do sleep 0.1
done
echo "${filename}"
