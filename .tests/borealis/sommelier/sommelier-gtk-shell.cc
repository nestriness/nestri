// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"          // NOLINT(build/include_directory)
#include "sommelier-tracing.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "aura-shell-client-protocol.h"  // NOLINT(build/include_directory)
#include "gtk-shell-server-protocol.h"   // NOLINT(build/include_directory)

struct sl_host_gtk_shell {
  struct sl_aura_shell* aura_shell;
  struct wl_resource* resource;
  struct zaura_shell* proxy;
  struct wl_callback* callback;
  char* startup_id;
  struct wl_list surfaces;
};

struct sl_host_gtk_surface {
  struct wl_resource* resource;
  struct zaura_surface* proxy;
  struct wl_list link;
  struct sl_aura_shell* aura_shell;
};

static void sl_gtk_surface_set_dbus_properties(
    struct wl_client* client,
    struct wl_resource* resource,
    const char* application_id,
    const char* app_menu_path,
    const char* menubar_path,
    const char* window_object_path,
    const char* application_object_path,
    const char* unique_bus_name) {
  struct sl_host_gtk_surface* host =
      static_cast<sl_host_gtk_surface*>(wl_resource_get_user_data(resource));

  char* application_id_str =
      sl_xasprintf(NATIVE_WAYLAND_APPLICATION_ID_FORMAT,
                   host->aura_shell->ctx->vm_id, application_id);

  zaura_surface_set_application_id(host->proxy, application_id_str);
}

static const struct gtk_surface1_interface sl_gtk_surface_implementation = {
    sl_gtk_surface_set_dbus_properties, /*set_modal=*/DoNothing,
    /*unset_modal=*/DoNothing, /*present=*/DoNothing};

static void sl_destroy_host_gtk_surface(struct wl_resource* resource) {
  struct sl_host_gtk_surface* host =
      static_cast<sl_host_gtk_surface*>(wl_resource_get_user_data(resource));

  zaura_surface_destroy(host->proxy);
  wl_list_remove(&host->link);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_gtk_shell_get_gtk_surface(struct wl_client* client,
                                         struct wl_resource* resource,
                                         uint32_t id,
                                         struct wl_resource* surface_resource) {
  struct sl_host_gtk_shell* host =
      static_cast<sl_host_gtk_shell*>(wl_resource_get_user_data(resource));
  struct sl_host_surface* host_surface = static_cast<sl_host_surface*>(
      wl_resource_get_user_data(surface_resource));
  struct sl_host_gtk_surface* host_gtk_surface = new sl_host_gtk_surface();

  wl_list_insert(&host->surfaces, &host_gtk_surface->link);
  host_gtk_surface->aura_shell = host->aura_shell;
  host_gtk_surface->resource =
      wl_resource_create(client, &gtk_surface1_interface, 1, id);
  wl_resource_set_implementation(host_gtk_surface->resource,
                                 &sl_gtk_surface_implementation,
                                 host_gtk_surface, sl_destroy_host_gtk_surface);
  host_gtk_surface->proxy =
      zaura_shell_get_aura_surface(host->proxy, host_surface->proxy);
  zaura_surface_set_startup_id(host_gtk_surface->proxy, host->startup_id);
}

// TODO(b/244651040): when adding changing the startup id format, also add vm_id
// here.
static void sl_gtk_shell_set_startup_id(struct wl_client* client,
                                        struct wl_resource* resource,
                                        const char* startup_id) {
  struct sl_host_gtk_shell* host =
      static_cast<sl_host_gtk_shell*>(wl_resource_get_user_data(resource));
  struct sl_host_gtk_surface* surface;

  free(host->startup_id);
  host->startup_id = startup_id ? strdup(startup_id) : nullptr;

  wl_list_for_each(surface, &host->surfaces, link)
      zaura_surface_set_startup_id(surface->proxy, host->startup_id);
}

static const struct gtk_shell1_interface sl_gtk_shell_implementation = {
    sl_gtk_shell_get_gtk_surface, sl_gtk_shell_set_startup_id,
    /*system_bell=*/DoNothing};

static void sl_destroy_host_gtk_shell(struct wl_resource* resource) {
  struct sl_host_gtk_shell* host =
      static_cast<sl_host_gtk_shell*>(wl_resource_get_user_data(resource));

  free(host->startup_id);
  wl_callback_destroy(host->callback);
  zaura_shell_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_gtk_shell_callback_done(void* data,
                                       struct wl_callback* callback,
                                       uint32_t serial) {
  TRACE_EVENT("shell", "sl_gtk_shell_callback_done");
  struct sl_host_gtk_shell* host =
      static_cast<sl_host_gtk_shell*>(wl_callback_get_user_data(callback));

  gtk_shell1_send_capabilities(host->resource, 0);
}

static const struct wl_callback_listener sl_gtk_shell_callback_listener = {
    sl_gtk_shell_callback_done};

static void sl_bind_host_gtk_shell(struct wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_gtk_shell* host = new sl_host_gtk_shell();
  host->aura_shell = ctx->aura_shell;
  host->startup_id = nullptr;
  wl_list_init(&host->surfaces);
  host->resource = wl_resource_create(client, &gtk_shell1_interface, 1, id);
  wl_resource_set_implementation(host->resource, &sl_gtk_shell_implementation,
                                 host, sl_destroy_host_gtk_shell);
  host->proxy = static_cast<zaura_shell*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), ctx->aura_shell->id,
      &zaura_shell_interface, ctx->aura_shell->version));
  zaura_shell_set_user_data(host->proxy, host);

  host->callback = wl_display_sync(ctx->aura_shell->ctx->display);
  wl_callback_add_listener(host->callback, &sl_gtk_shell_callback_listener,
                           host);
}

struct sl_global* sl_gtk_shell_global_create(struct sl_context* ctx) {
  return sl_global_create(ctx, &gtk_shell1_interface, 1, ctx,
                          sl_bind_host_gtk_shell);
}
