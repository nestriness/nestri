// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-global.h"  // NOLINT(build/include_directory)

#include <assert.h>

#include "sommelier.h"          // NOLINT(build/include_directory)
#include "sommelier-tracing.h"  // NOLINT(build/include_directory)

struct sl_global* sl_global_create(struct sl_context* ctx,
                                   const struct wl_interface* interface,
                                   int version,
                                   void* data,
                                   wl_global_bind_func_t bind) {
  TRACE_EVENT("other", "sl_global_create");
  struct sl_host_registry* registry;

  assert(version > 0);
  assert(version <= interface->version);

  struct sl_global* global = static_cast<sl_global*>(malloc(sizeof *global));
  assert(global);

  global->ctx = ctx;
  global->name = ctx->next_global_id++;
  global->interface = interface;
  global->version = version;
  global->data = data;
  global->bind = bind;
  wl_list_insert(ctx->globals.prev, &global->link);

  wl_list_for_each(registry, &ctx->registries, link) {
    wl_resource_post_event(registry->resource, WL_REGISTRY_GLOBAL, global->name,
                           global->interface->name, global->version);
  }

  return global;
}
