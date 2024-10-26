// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"            // NOLINT(build/include_directory)
#include "sommelier-transform.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <stdlib.h>
#include <wayland-client.h>

struct sl_host_subcompositor {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_subcompositor* proxy;
};

struct sl_host_subsurface {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_subsurface* proxy;
};
MAP_STRUCTS(wl_subsurface, sl_host_subsurface);

static void sl_subsurface_destroy(struct wl_client* client,
                                  struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_subsurface_set_position(struct wl_client* client,
                                       struct wl_resource* resource,
                                       int32_t x,
                                       int32_t y) {
  struct sl_host_subsurface* host =
      static_cast<sl_host_subsurface*>(wl_resource_get_user_data(resource));
  int32_t ix = x;
  int32_t iy = y;

  sl_transform_guest_to_host(host->ctx, nullptr, &ix, &iy);
  wl_subsurface_set_position(host->proxy, ix, iy);
}

static const struct wl_subsurface_interface sl_subsurface_implementation = {
    sl_subsurface_destroy,
    sl_subsurface_set_position,
    ForwardRequest<wl_subsurface_place_above>,
    ForwardRequest<wl_subsurface_place_below>,
    ForwardRequest<wl_subsurface_set_sync>,
    ForwardRequest<wl_subsurface_set_desync>,
};

static void sl_destroy_host_subsurface(struct wl_resource* resource) {
  struct sl_host_subsurface* host =
      static_cast<sl_host_subsurface*>(wl_resource_get_user_data(resource));

  wl_subsurface_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_subcompositor_destroy(struct wl_client* client,
                                     struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_subcompositor_get_subsurface(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* surface_resource,
    struct wl_resource* parent_resource) {
  struct sl_host_subcompositor* host =
      static_cast<sl_host_subcompositor*>(wl_resource_get_user_data(resource));
  struct sl_host_surface* host_surface = static_cast<sl_host_surface*>(
      wl_resource_get_user_data(surface_resource));
  struct sl_host_surface* host_parent =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(parent_resource));
  struct sl_host_subsurface* host_subsurface = new sl_host_subsurface();

  host_subsurface->ctx = host->ctx;
  host_subsurface->resource =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);
  wl_resource_set_implementation(host_subsurface->resource,
                                 &sl_subsurface_implementation, host_subsurface,
                                 sl_destroy_host_subsurface);
  host_subsurface->proxy = wl_subcompositor_get_subsurface(
      host->proxy, host_surface->proxy, host_parent->proxy);
  wl_subsurface_set_user_data(host_subsurface->proxy, host_subsurface);
  host_surface->has_role = 1;
}

static const struct wl_subcompositor_interface sl_subcompositor_implementation =
    {sl_subcompositor_destroy, sl_subcompositor_get_subsurface};

static void sl_destroy_host_subcompositor(struct wl_resource* resource) {
  struct sl_host_subcompositor* host =
      static_cast<sl_host_subcompositor*>(wl_resource_get_user_data(resource));

  wl_subcompositor_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_bind_host_subcompositor(struct wl_client* client,
                                       void* data,
                                       uint32_t version,
                                       uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_subcompositor* host = new sl_host_subcompositor();
  host->ctx = ctx;
  host->resource =
      wl_resource_create(client, &wl_subcompositor_interface, 1, id);
  wl_resource_set_implementation(host->resource,
                                 &sl_subcompositor_implementation, host,
                                 sl_destroy_host_subcompositor);
  host->proxy = static_cast<wl_subcompositor*>(
      wl_registry_bind(wl_display_get_registry(ctx->display),
                       ctx->subcompositor->id, &wl_subcompositor_interface, 1));
  wl_subcompositor_set_user_data(host->proxy, host);
}

struct sl_global* sl_subcompositor_global_create(struct sl_context* ctx) {
  return sl_global_create(ctx, &wl_subcompositor_interface, 1, ctx,
                          sl_bind_host_subcompositor);
}
