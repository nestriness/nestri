// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-xdg-shell.h"  // NOLINT(build/include_directory)

#include <algorithm>
#include <assert.h>
#include <stdlib.h>

#include "sommelier.h"                  // NOLINT(build/include_directory)
#include "sommelier-transform.h"        // NOLINT(build/include_directory)
#include "xdg-shell-client-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-shell-server-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-shell-shim.h"             // NOLINT(build/include_directory)

static void sl_xdg_positioner_destroy(struct wl_client* client,
                                      struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_xdg_positioner_set_size(struct wl_client* client,
                                       struct wl_resource* resource,
                                       int32_t width,
                                       int32_t height) {
  struct sl_host_xdg_positioner* host =
      static_cast<sl_host_xdg_positioner*>(wl_resource_get_user_data(resource));

  int32_t iwidth = width;
  int32_t iheight = height;

  sl_transform_guest_to_host(host->ctx, nullptr, &iwidth, &iheight);

  xdg_positioner_shim()->set_size(host->proxy, iwidth, iheight);
}

static void sl_xdg_positioner_set_anchor_rect(struct wl_client* client,
                                              struct wl_resource* resource,
                                              int32_t x,
                                              int32_t y,
                                              int32_t width,
                                              int32_t height) {
  struct sl_host_xdg_positioner* host =
      static_cast<sl_host_xdg_positioner*>(wl_resource_get_user_data(resource));
  int32_t x1 = x;
  int32_t y1 = y;
  int32_t x2 = x + width;
  int32_t y2 = y + height;

  sl_transform_guest_to_host(host->ctx, nullptr, &x1, &y1);
  sl_transform_guest_to_host(host->ctx, nullptr, &x2, &y2);

  xdg_positioner_shim()->set_anchor_rect(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void sl_xdg_positioner_set_offset(struct wl_client* client,
                                         struct wl_resource* resource,
                                         int32_t x,
                                         int32_t y) {
  struct sl_host_xdg_positioner* host =
      static_cast<sl_host_xdg_positioner*>(wl_resource_get_user_data(resource));
  int32_t ix = x, iy = y;

  sl_transform_guest_to_host(host->ctx, nullptr, &ix, &iy);
  xdg_positioner_shim()->set_offset(host->proxy, ix, iy);
}

static const struct xdg_positioner_interface sl_xdg_positioner_implementation =
    {sl_xdg_positioner_destroy,
     sl_xdg_positioner_set_size,
     sl_xdg_positioner_set_anchor_rect,
     ForwardRequestToShim<xdg_positioner_shim, &XdgPositionerShim::set_anchor>,
     ForwardRequestToShim<xdg_positioner_shim, &XdgPositionerShim::set_gravity>,
     ForwardRequestToShim<xdg_positioner_shim,
                          &XdgPositionerShim::set_constraint_adjustment>,
     sl_xdg_positioner_set_offset,
     ForwardRequestToShim<xdg_positioner_shim,
                          &XdgPositionerShim::set_reactive>,
     ForwardRequestToShim<xdg_positioner_shim,
                          &XdgPositionerShim::set_parent_size>,
     ForwardRequestToShim<xdg_positioner_shim,
                          &XdgPositionerShim::set_parent_configure>};

static void sl_destroy_host_xdg_positioner(struct wl_resource* resource) {
  struct sl_host_xdg_positioner* host =
      static_cast<sl_host_xdg_positioner*>(wl_resource_get_user_data(resource));

  xdg_positioner_shim()->destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_xdg_popup_destroy(struct wl_client* client,
                                 struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct xdg_popup_interface sl_xdg_popup_implementation = {
    sl_xdg_popup_destroy,
    ForwardRequestToShim<xdg_popup_shim, &XdgPopupShim::grab>,
    ForwardRequestToShim<xdg_popup_shim, &XdgPopupShim::reposition>};

static struct sl_host_surface* get_host_surface(
    struct sl_host_xdg_surface* xdg) {
  // For xdg_popup/xdg_toplevel they will point to the
  // originating xdg_surface. The originating surface
  // will point to the source sl_host_surface
  if (xdg && xdg->originator)
    return xdg->originator;
  else
    return nullptr;
}

static void sl_xdg_popup_configure(void* data,
                                   struct xdg_popup* xdg_popup,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  struct sl_host_xdg_popup* host = static_cast<sl_host_xdg_popup*>(
      xdg_popup_shim()->get_user_data(xdg_popup));
  int32_t x1 = x;
  int32_t y1 = y;
  int32_t x2 = x + width;
  int32_t y2 = y + height;

  sl_transform_host_to_guest(host->ctx, get_host_surface(host->originator), &x1,
                             &y1);
  sl_transform_host_to_guest(host->ctx, get_host_surface(host->originator), &x2,
                             &y2);

  xdg_popup_shim()->send_configure(host->resource, x1, y1, x2 - x1, y2 - y1);
}

static void sl_xdg_popup_popup_done(void* data, struct xdg_popup* xdg_popup) {
  struct sl_host_xdg_popup* host = static_cast<sl_host_xdg_popup*>(
      xdg_popup_shim()->get_user_data(xdg_popup));

  xdg_popup_shim()->send_popup_done(host->resource);
}

static void sl_xdg_popup_repositioned(void* data,
                                      struct xdg_popup* xdg_popup,
                                      uint32_t token) {
  struct sl_host_xdg_popup* host = static_cast<sl_host_xdg_popup*>(
      xdg_popup_shim()->get_user_data(xdg_popup));
  xdg_popup_shim()->send_repositioned(host->resource, token);
}

static const struct xdg_popup_listener sl_xdg_popup_listener = {
    sl_xdg_popup_configure, sl_xdg_popup_popup_done, sl_xdg_popup_repositioned};

static void sl_destroy_host_xdg_popup(struct wl_resource* resource) {
  struct sl_host_xdg_popup* host =
      static_cast<sl_host_xdg_popup*>(wl_resource_get_user_data(resource));

  xdg_popup_shim()->destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_xdg_toplevel_destroy(struct wl_client* client,
                                    struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_xdg_toplevel_show_window_menu(struct wl_client* client,
                                             struct wl_resource* resource,
                                             struct wl_resource* seat_resource,
                                             uint32_t serial,
                                             int32_t x,
                                             int32_t y) {
  struct sl_host_xdg_toplevel* host =
      static_cast<sl_host_xdg_toplevel*>(wl_resource_get_user_data(resource));
  struct sl_host_seat* host_seat =
      seat_resource
          ? static_cast<sl_host_seat*>(wl_resource_get_user_data(seat_resource))
          : nullptr;

  // TODO(mrisaacb): There was no scaling performed here in the original code.
  // Figure out why this was.
  xdg_toplevel_shim()->show_window_menu(
      host->proxy, host_seat ? host_seat->proxy : nullptr, serial, x, y);
}

static void sl_xdg_toplevel_set_app_id(struct wl_client* client,
                                       struct wl_resource* resource,
                                       const char* app_id) {
  struct sl_host_xdg_toplevel* host =
      static_cast<sl_host_xdg_toplevel*>(wl_resource_get_user_data(resource));
  char* application_id_str = sl_xasprintf(NATIVE_WAYLAND_APPLICATION_ID_FORMAT,
                                          host->ctx->vm_id, app_id);
  xdg_toplevel_shim()->set_app_id(host->proxy, application_id_str);
}

static const struct xdg_toplevel_interface sl_xdg_toplevel_implementation = {
    sl_xdg_toplevel_destroy,
    ForwardRequestToShim<xdg_toplevel_shim,
                         &XdgToplevelShim::set_parent,
                         AllowNullResource::kYes>,
    ForwardRequestToShim<xdg_toplevel_shim, &XdgToplevelShim::set_title>,
    sl_xdg_toplevel_set_app_id,
    sl_xdg_toplevel_show_window_menu,
    ForwardRequestToShim<xdg_toplevel_shim,
                         &XdgToplevelShim::move,
                         AllowNullResource::kYes>,
    ForwardRequestToShim<xdg_toplevel_shim,
                         &XdgToplevelShim::resize,
                         AllowNullResource::kYes>,
    ForwardRequestToShim<xdg_toplevel_shim, &XdgToplevelShim::set_max_size>,
    ForwardRequestToShim<xdg_toplevel_shim, &XdgToplevelShim::set_min_size>,
    ForwardRequestToShim<xdg_toplevel_shim, &XdgToplevelShim::set_maximized>,
    ForwardRequestToShim<xdg_toplevel_shim, &XdgToplevelShim::unset_maximized>,
    ForwardRequestToShim<xdg_toplevel_shim,
                         &XdgToplevelShim::set_fullscreen,
                         AllowNullResource::kYes>,
    ForwardRequestToShim<xdg_toplevel_shim, &XdgToplevelShim::unset_fullscreen>,
    ForwardRequestToShim<xdg_toplevel_shim, &XdgToplevelShim::set_minimized>,
};

static void sl_xdg_toplevel_configure(void* data,
                                      struct xdg_toplevel* xdg_toplevel,
                                      int32_t width,
                                      int32_t height,
                                      struct wl_array* states) {
  struct sl_host_xdg_toplevel* host = static_cast<sl_host_xdg_toplevel*>(
      xdg_toplevel_shim()->get_user_data(xdg_toplevel));

  int32_t iwidth = width;
  int32_t iheight = height;

  sl_transform_host_to_guest(host->ctx, get_host_surface(host->originator),
                             &iwidth, &iheight);

  xdg_toplevel_shim()->send_configure(host->resource, iwidth, iheight, states);
}

static void sl_xdg_toplevel_close(void* data,
                                  struct xdg_toplevel* xdg_toplevel) {
  struct sl_host_xdg_toplevel* host = static_cast<sl_host_xdg_toplevel*>(
      xdg_toplevel_shim()->get_user_data(xdg_toplevel));

  xdg_toplevel_shim()->send_close(host->resource);
}

static const struct xdg_toplevel_listener sl_xdg_toplevel_listener = {
    sl_xdg_toplevel_configure, sl_xdg_toplevel_close};

static void sl_destroy_host_xdg_toplevel(struct wl_resource* resource) {
  struct sl_host_xdg_toplevel* host =
      static_cast<sl_host_xdg_toplevel*>(wl_resource_get_user_data(resource));

  xdg_toplevel_shim()->destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_xdg_surface_destroy(struct wl_client* client,
                                   struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_xdg_surface_get_toplevel(struct wl_client* client,
                                        struct wl_resource* resource,
                                        uint32_t id) {
  struct sl_host_xdg_surface* host =
      static_cast<sl_host_xdg_surface*>(wl_resource_get_user_data(resource));
  struct sl_host_xdg_toplevel* host_xdg_toplevel = new sl_host_xdg_toplevel();

  host_xdg_toplevel->ctx = host->ctx;
  host_xdg_toplevel->resource = wl_resource_create(
      client, &xdg_toplevel_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(
      host_xdg_toplevel->resource, &sl_xdg_toplevel_implementation,
      host_xdg_toplevel, sl_destroy_host_xdg_toplevel);
  host_xdg_toplevel->proxy = xdg_surface_shim()->get_toplevel(host->proxy);
  host_xdg_toplevel->originator = host;

  xdg_toplevel_shim()->add_listener(
      host_xdg_toplevel->proxy, &sl_xdg_toplevel_listener, host_xdg_toplevel);
}

static void sl_xdg_surface_get_popup(struct wl_client* client,
                                     struct wl_resource* resource,
                                     uint32_t id,
                                     struct wl_resource* parent_resource,
                                     struct wl_resource* positioner_resource) {
  struct sl_host_xdg_surface* host =
      static_cast<sl_host_xdg_surface*>(wl_resource_get_user_data(resource));
  struct sl_host_xdg_surface* host_parent =
      parent_resource ? static_cast<sl_host_xdg_surface*>(
                            wl_resource_get_user_data(parent_resource))
                      : nullptr;
  struct sl_host_xdg_positioner* host_positioner =
      static_cast<sl_host_xdg_positioner*>(
          wl_resource_get_user_data(positioner_resource));
  struct sl_host_xdg_popup* host_xdg_popup = new sl_host_xdg_popup();

  host_xdg_popup->ctx = host->ctx;
  host_xdg_popup->resource = wl_resource_create(
      client, &xdg_popup_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_xdg_popup->resource,
                                 &sl_xdg_popup_implementation, host_xdg_popup,
                                 sl_destroy_host_xdg_popup);
  host_xdg_popup->proxy = xdg_surface_shim()->get_popup(
      host->proxy, host_parent ? host_parent->proxy : nullptr,
      host_positioner->proxy);
  host_xdg_popup->originator = host_parent;

  xdg_popup_shim()->add_listener(host_xdg_popup->proxy, &sl_xdg_popup_listener,
                                 host_xdg_popup);
}

static void sl_xdg_surface_set_window_geometry(struct wl_client* client,
                                               struct wl_resource* resource,
                                               int32_t x,
                                               int32_t y,
                                               int32_t width,
                                               int32_t height) {
  struct sl_host_xdg_surface* host =
      static_cast<sl_host_xdg_surface*>(wl_resource_get_user_data(resource));
  int32_t x1 = x;
  int32_t y1 = y;
  int32_t x2 = x + width;
  int32_t y2 = y + height;

  sl_transform_guest_to_host(host->ctx, host->originator, &x1, &y1);
  sl_transform_guest_to_host(host->ctx, host->originator, &x2, &y2);

  xdg_surface_shim()->set_window_geometry(host->proxy, x1, y1, x2 - x1,
                                          y2 - y1);
}

static const struct xdg_surface_interface sl_xdg_surface_implementation = {
    sl_xdg_surface_destroy, sl_xdg_surface_get_toplevel,
    sl_xdg_surface_get_popup, sl_xdg_surface_set_window_geometry,
    ForwardRequestToShim<xdg_surface_shim, &XdgSurfaceShim::ack_configure>};

static void sl_xdg_surface_configure(void* data,
                                     struct xdg_surface* xdg_surface,
                                     uint32_t serial) {
  struct sl_host_xdg_surface* host = static_cast<sl_host_xdg_surface*>(
      xdg_surface_shim()->get_user_data(xdg_surface));

  xdg_surface_shim()->send_configure(host->resource, serial);
}

static const struct xdg_surface_listener sl_xdg_surface_listener = {
    sl_xdg_surface_configure};

static void sl_destroy_host_xdg_surface(struct wl_resource* resource) {
  struct sl_host_xdg_surface* host =
      static_cast<sl_host_xdg_surface*>(wl_resource_get_user_data(resource));

  xdg_surface_shim()->destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_xdg_shell_destroy(struct wl_client* client,
                                 struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_xdg_shell_create_positioner(struct wl_client* client,
                                           struct wl_resource* resource,
                                           uint32_t id) {
  struct sl_host_xdg_shell* host =
      static_cast<sl_host_xdg_shell*>(wl_resource_get_user_data(resource));
  struct sl_host_xdg_positioner* host_xdg_positioner =
      new sl_host_xdg_positioner();

  host_xdg_positioner->ctx = host->ctx;
  host_xdg_positioner->resource = wl_resource_create(
      client, &xdg_positioner_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(
      host_xdg_positioner->resource, &sl_xdg_positioner_implementation,
      host_xdg_positioner, sl_destroy_host_xdg_positioner);
  host_xdg_positioner->proxy =
      xdg_wm_base_shim()->create_positioner(host->proxy);
  xdg_positioner_shim()->set_user_data(host_xdg_positioner->proxy,
                                       host_xdg_positioner);
}

static void sl_xdg_shell_get_xdg_surface(struct wl_client* client,
                                         struct wl_resource* resource,
                                         uint32_t id,
                                         struct wl_resource* surface_resource) {
  struct sl_host_xdg_shell* host =
      static_cast<sl_host_xdg_shell*>(wl_resource_get_user_data(resource));
  struct sl_host_surface* host_surface = static_cast<sl_host_surface*>(
      wl_resource_get_user_data(surface_resource));
  struct sl_host_xdg_surface* host_xdg_surface = new sl_host_xdg_surface();

  host_xdg_surface->ctx = host->ctx;
  host_xdg_surface->resource = wl_resource_create(
      client, &xdg_surface_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_xdg_surface->resource,
                                 &sl_xdg_surface_implementation,
                                 host_xdg_surface, sl_destroy_host_xdg_surface);
  host_xdg_surface->proxy =
      xdg_wm_base_shim()->get_xdg_surface(host->proxy, host_surface->proxy);
  host_xdg_surface->originator = host_surface;

  xdg_surface_shim()->add_listener(host_xdg_surface->proxy,
                                   &sl_xdg_surface_listener, host_xdg_surface);
  host_surface->has_role = 1;
}

static const struct xdg_wm_base_interface sl_xdg_shell_implementation = {
    sl_xdg_shell_destroy, sl_xdg_shell_create_positioner,
    sl_xdg_shell_get_xdg_surface,
    ForwardRequestToShim<xdg_wm_base_shim, &XdgWmBaseShim::pong>};

static void sl_xdg_shell_ping(void* data,
                              struct xdg_wm_base* xdg_shell,
                              uint32_t serial) {
  struct sl_host_xdg_shell* host = static_cast<sl_host_xdg_shell*>(
      xdg_wm_base_shim()->get_user_data(xdg_shell));

  xdg_wm_base_shim()->send_ping(host->resource, serial);
}

static const struct xdg_wm_base_listener sl_xdg_shell_listener = {
    sl_xdg_shell_ping};

static void sl_destroy_host_xdg_shell(struct wl_resource* resource) {
  struct sl_host_xdg_shell* host =
      static_cast<sl_host_xdg_shell*>(wl_resource_get_user_data(resource));

  xdg_wm_base_shim()->destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_bind_host_xdg_shell(struct wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_xdg_shell* host = new sl_host_xdg_shell();
  host->ctx = ctx;
  host->resource =
      wl_resource_create(client, &xdg_wm_base_interface, version, id);
  wl_resource_set_implementation(host->resource, &sl_xdg_shell_implementation,
                                 host, sl_destroy_host_xdg_shell);
  host->proxy = static_cast<xdg_wm_base*>(
      wl_registry_bind(wl_display_get_registry(ctx->display),
                       ctx->xdg_shell->id, &xdg_wm_base_interface, version));
  xdg_wm_base_shim()->add_listener(host->proxy, &sl_xdg_shell_listener, host);
}

struct sl_global* sl_xdg_shell_global_create(struct sl_context* ctx,
                                             uint32_t version) {
  return sl_global_create(ctx, &xdg_wm_base_interface, version, ctx,
                          sl_bind_host_xdg_shell);
}
