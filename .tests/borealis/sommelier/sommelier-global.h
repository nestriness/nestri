// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_GLOBAL_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_GLOBAL_H_

#include <wayland-server-core.h>

struct sl_global* sl_global_create(struct sl_context* ctx,
                                   const struct wl_interface* interface,
                                   int version,
                                   void* data,
                                   wl_global_bind_func_t bind);

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_GLOBAL_H_
