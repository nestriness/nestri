// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unlike the other protocol handlers, this one talks the stylus-unstable-v2
// protocol to to the host, but exposes the tablet-unstable-v2 protocol
// to the clients.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_STYLUS_TABLET_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_STYLUS_TABLET_H_

#include "sommelier-inpututils.h"  // NOLINT(build/include_directory)

struct sl_host_stylus_tablet;

void sl_host_stylus_tablet_handle_touch(struct sl_host_stylus_tablet* tablet,
                                        struct sl_touchrecorder* recorder);

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_STYLUS_TABLET_H_
