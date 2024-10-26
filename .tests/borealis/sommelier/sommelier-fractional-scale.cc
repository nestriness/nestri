// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"          // NOLINT(build/include_directory)
#include "sommelier-tracing.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <stdlib.h>

#include "fractional-scale-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "fractional-scale-v1-server-protocol.h"  // NOLINT(build/include_directory)

struct sl_host_fractional_scale_manager {
  struct sl_fractional_scale_manager* fractional_scale_manager;
  struct wl_resource* resource;
  struct wp_fractional_scale_manager_v1* proxy;
};

struct sl_host_fractional_scale {
  struct sl_host_fractional_scale_manager* host_fractional_scale_manager;
  struct wl_resource* resource;
  struct wp_fractional_scale_v1* proxy;
};

static void sl_fractional_scale_destroy(struct wl_client* client,
                                        struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_fractional_scale_handle_preferred_scale(
    void* data,
    struct wp_fractional_scale_v1* fractional_scale,
    uint32_t wire_scale) {
  struct sl_host_fractional_scale* host_fractional_scale =
      static_cast<sl_host_fractional_scale*>(
          wp_fractional_scale_v1_get_user_data(fractional_scale));
  struct sl_fractional_scale_manager* fractional_scale_manager =
      host_fractional_scale->host_fractional_scale_manager
          ->fractional_scale_manager;

  wp_fractional_scale_v1_send_preferred_scale(
      host_fractional_scale->resource,
      round(wire_scale / fractional_scale_manager->ctx->scale));
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener =
    {
        .preferred_scale = sl_fractional_scale_handle_preferred_scale,
};

static const struct wp_fractional_scale_v1_interface
    sl_fractional_scale_implementation = {sl_fractional_scale_destroy};

static void sl_destroy_host_fractional_scale(struct wl_resource* resource) {
  struct sl_host_fractional_scale* host =
      static_cast<sl_host_fractional_scale*>(
          wl_resource_get_user_data(resource));

  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_fractional_scale_manager_destroy(struct wl_client* client,
                                                struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_fractional_scale_manager_get_fractional_scale(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* surface_resource) {
  struct sl_host_fractional_scale_manager* host =
      static_cast<sl_host_fractional_scale_manager*>(
          wl_resource_get_user_data(resource));
  struct sl_host_surface* host_surface = static_cast<sl_host_surface*>(
      wl_resource_get_user_data(surface_resource));
  struct sl_host_fractional_scale* host_fractional_scale =
      new sl_host_fractional_scale();

  host_fractional_scale->host_fractional_scale_manager = host;
  host_fractional_scale->resource =
      wl_resource_create(client, &wp_fractional_scale_v1_interface, 1, id);
  host_fractional_scale->proxy =
      wp_fractional_scale_manager_v1_get_fractional_scale(
          host->fractional_scale_manager->internal, host_surface->proxy);
  wp_fractional_scale_v1_add_listener(host_fractional_scale->proxy,
                                      &fractional_scale_listener,
                                      host_fractional_scale);
  wl_resource_set_implementation(
      host_fractional_scale->resource, &sl_fractional_scale_implementation,
      host_fractional_scale, sl_destroy_host_fractional_scale);
}

static const struct wp_fractional_scale_manager_v1_interface
    sl_wp_fractional_scale_manager_implementation = {
        sl_fractional_scale_manager_destroy,
        sl_fractional_scale_manager_get_fractional_scale,
};

static void sl_destroy_host_fractional_scale_manager(
    struct wl_resource* resource) {
  struct sl_host_fractional_scale_manager* host =
      static_cast<sl_host_fractional_scale_manager*>(
          wl_resource_get_user_data(resource));

  wp_fractional_scale_manager_v1_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_bind_host_fractional_scale_manager(struct wl_client* client,
                                                  void* data,
                                                  uint32_t version,
                                                  uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_fractional_scale_manager* host =
      new sl_host_fractional_scale_manager();
  host->fractional_scale_manager = ctx->fractional_scale_manager;
  host->resource = wl_resource_create(
      client, &wp_fractional_scale_manager_v1_interface, 1, id);
  wl_resource_set_implementation(
      host->resource, &sl_wp_fractional_scale_manager_implementation, host,
      sl_destroy_host_fractional_scale_manager);
  host->proxy = static_cast<wp_fractional_scale_manager_v1*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), ctx->fractional_scale_manager->id,
      &wp_fractional_scale_manager_v1_interface, 1));
  wp_fractional_scale_manager_v1_set_user_data(host->proxy, host);
}

struct sl_global* sl_fractional_scale_manager_global_create(
    struct sl_context* ctx) {
  return sl_global_create(ctx, &wp_fractional_scale_manager_v1_interface, 1,
                          ctx, sl_bind_host_fractional_scale_manager);
}
