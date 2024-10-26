// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"          // NOLINT(build/include_directory)
#include "sommelier-tracing.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <stdlib.h>

#include "sommelier-logging.h"           // NOLINT(build/include_directory)
#include "viewporter-client-protocol.h"  // NOLINT(build/include_directory)
#include "viewporter-server-protocol.h"  // NOLINT(build/include_directory)

struct sl_host_viewporter {
  struct sl_viewporter* viewporter;
  struct wl_resource* resource;
  struct wp_viewporter* proxy;
};

struct sl_host_viewport {
  struct wl_resource* resource;
  struct sl_viewport viewport;
};

static void sl_viewport_destroy(struct wl_client* client,
                                struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_viewport_set_source(struct wl_client* client,
                                   struct wl_resource* resource,
                                   wl_fixed_t x,
                                   wl_fixed_t y,
                                   wl_fixed_t width,
                                   wl_fixed_t height) {
  struct sl_host_viewport* host =
      static_cast<sl_host_viewport*>(wl_resource_get_user_data(resource));

  host->viewport.src_x = x;
  host->viewport.src_y = y;
  host->viewport.src_width = width;
  host->viewport.src_height = height;
  LOG(VERBOSE) << "viewport src set " << wl_fixed_to_int(width) << "x"
               << wl_fixed_to_int(height) << " (" << wl_fixed_to_int(x) << ","
               << wl_fixed_to_int(y) << ")";
}

static void sl_viewport_set_destination(struct wl_client* client,
                                        struct wl_resource* resource,
                                        int32_t width,
                                        int32_t height) {
  TRACE_EVENT("viewport", "sl_viewport_set_destination", "resource_id",
              wl_resource_get_id(resource));
  struct sl_host_viewport* host =
      static_cast<sl_host_viewport*>(wl_resource_get_user_data(resource));

  host->viewport.dst_width = width;
  host->viewport.dst_height = height;
  LOG(VERBOSE) << "viewport dst set " << width << "x" << height;
}

static const struct wp_viewport_interface sl_viewport_implementation = {
    sl_viewport_destroy, sl_viewport_set_source, sl_viewport_set_destination};

static void sl_destroy_host_viewport(struct wl_resource* resource) {
  struct sl_host_viewport* host =
      static_cast<sl_host_viewport*>(wl_resource_get_user_data(resource));

  wl_resource_set_user_data(resource, nullptr);
  wl_list_remove(&host->viewport.link);
  delete host;
}

static void sl_viewporter_destroy(struct wl_client* client,
                                  struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_viewporter_get_viewport(struct wl_client* client,
                                       struct wl_resource* resource,
                                       uint32_t id,
                                       struct wl_resource* surface_resource) {
  struct sl_host_surface* host_surface = static_cast<sl_host_surface*>(
      wl_resource_get_user_data(surface_resource));
  struct sl_host_viewport* host_viewport = new sl_host_viewport();

  host_viewport->viewport.src_x = -1;
  host_viewport->viewport.src_y = -1;
  host_viewport->viewport.src_width = -1;
  host_viewport->viewport.src_height = -1;
  host_viewport->viewport.dst_width = -1;
  host_viewport->viewport.dst_height = -1;
  wl_list_insert(&host_surface->contents_viewport,
                 &host_viewport->viewport.link);
  host_viewport->resource =
      wl_resource_create(client, &wp_viewport_interface, 1, id);
  wl_resource_set_implementation(host_viewport->resource,
                                 &sl_viewport_implementation, host_viewport,
                                 sl_destroy_host_viewport);
}

static const struct wp_viewporter_interface sl_viewporter_implementation = {
    sl_viewporter_destroy, sl_viewporter_get_viewport};

static void sl_destroy_host_viewporter(struct wl_resource* resource) {
  struct sl_host_viewporter* host =
      static_cast<sl_host_viewporter*>(wl_resource_get_user_data(resource));

  wp_viewporter_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_bind_host_viewporter(struct wl_client* client,
                                    void* data,
                                    uint32_t version,
                                    uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_viewporter* host = new sl_host_viewporter();
  host->viewporter = ctx->viewporter;
  host->resource = wl_resource_create(client, &wp_viewporter_interface, 1, id);
  wl_resource_set_implementation(host->resource, &sl_viewporter_implementation,
                                 host, sl_destroy_host_viewporter);
  host->proxy = static_cast<wp_viewporter*>(
      wl_registry_bind(wl_display_get_registry(ctx->display),
                       ctx->viewporter->id, &wp_viewporter_interface, 1));
  wp_viewporter_set_user_data(host->proxy, host);
}

struct sl_global* sl_viewporter_global_create(struct sl_context* ctx) {
  return sl_global_create(ctx, &wp_viewporter_interface, 1, ctx,
                          sl_bind_host_viewporter);
}
