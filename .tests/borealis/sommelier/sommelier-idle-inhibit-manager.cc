// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "idle-inhibit-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "idle-inhibit-unstable-v1-server-protocol.h"  // NOLINT(build/include_directory)

struct sl_host_idle_inhibit_manager {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct zwp_idle_inhibit_manager_v1* proxy;
};

struct sl_host_idle_inhibitor {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct zwp_idle_inhibitor_v1* proxy;
};

static void sl_destroy_host_idle_inhibitor(struct wl_resource* resource) {
  struct sl_host_idle_inhibitor* host =
      static_cast<sl_host_idle_inhibitor*>(wl_resource_get_user_data(resource));

  zwp_idle_inhibitor_v1_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_idle_inhibit_destroy(struct wl_client* client,
                                    struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static struct zwp_idle_inhibitor_v1_interface sl_idle_inhibit_implementation = {
    sl_idle_inhibit_destroy,
};

static void sl_destroy_host_idle_inhibit_manager(struct wl_resource* resource) {
  struct sl_host_idle_inhibit_manager* host =
      static_cast<sl_host_idle_inhibit_manager*>(
          wl_resource_get_user_data(resource));

  zwp_idle_inhibit_manager_v1_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_idle_inhibit_manager_destroy(struct wl_client* client,
                                            struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_idle_inhibit_manager_create_inhibitor(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* surface) {
  struct sl_host_idle_inhibit_manager* host =
      static_cast<sl_host_idle_inhibit_manager*>(
          wl_resource_get_user_data(resource));
  struct sl_host_surface* host_surface =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(surface));
  struct wl_resource* idle_inhibit_resource =
      wl_resource_create(client, &zwp_idle_inhibitor_v1_interface, 1, id);
  struct sl_host_idle_inhibitor* idle_inhibitor_host =
      new sl_host_idle_inhibitor();
  idle_inhibitor_host->resource = idle_inhibit_resource;
  idle_inhibitor_host->ctx = host->ctx;
  idle_inhibitor_host->proxy = zwp_idle_inhibit_manager_v1_create_inhibitor(
      host->proxy, host_surface->proxy);
  wl_resource_set_implementation(
      idle_inhibit_resource, &sl_idle_inhibit_implementation,
      idle_inhibitor_host, sl_destroy_host_idle_inhibitor);
}

static struct zwp_idle_inhibit_manager_v1_interface
    sl_idle_inhibit_manager_implementation = {
        sl_idle_inhibit_manager_destroy,
        sl_idle_inhibit_manager_create_inhibitor,
};

static void sl_bind_host_idle_inhibit_manager(struct wl_client* client,
                                              void* data,
                                              uint32_t version,
                                              uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_idle_inhibit_manager* idle_inhibit_manager =
      ctx->idle_inhibit_manager;
  struct sl_host_idle_inhibit_manager* host =
      new sl_host_idle_inhibit_manager();
  host->ctx = ctx;
  host->resource =
      wl_resource_create(client, &zwp_idle_inhibit_manager_v1_interface, 1, id);
  wl_resource_set_implementation(host->resource,
                                 &sl_idle_inhibit_manager_implementation, host,
                                 sl_destroy_host_idle_inhibit_manager);
  host->proxy = static_cast<zwp_idle_inhibit_manager_v1*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), idle_inhibit_manager->id,
      &zwp_idle_inhibit_manager_v1_interface,
      wl_resource_get_version(host->resource)));
  zwp_idle_inhibit_manager_v1_set_user_data(host->proxy, host);
}

struct sl_global* sl_idle_inhibit_manager_global_create(
    struct sl_context* ctx) {
  return sl_global_create(ctx, &zwp_idle_inhibit_manager_v1_interface, 1, ctx,
                          sl_bind_host_idle_inhibit_manager);
}
