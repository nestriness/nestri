// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "compositor/sommelier-linux-dmabuf.h"  // NOLINT(build/include_directory)
#include "sommelier.h"  // NOLINT(build/include_directory)
#include <cstdint>
#include <cstring>
#include "sommelier-logging.h"      // NOLINT(build/include_directory)
#include "sommelier-scope-timer.h"  // NOLINT(build/include_directory)
#include "sommelier-tracing.h"      // NOLINT(build/include_directory)
#include "sommelier-transform.h"    // NOLINT(build/include_directory)
#include "sommelier-window.h"       // NOLINT(build/include_directory)
#include "sommelier-xdg-shell.h"    // NOLINT(build/include_directory)
#include "sommelier-xshape.h"       // NOLINT(build/include_directory)
#ifdef GAMEPAD_SUPPORT
#include "libevdev/libevdev-shim.h"
#endif
#include "viewporter-shim.h"  // NOLINT(build/include_directory)
#include "xcb/xcb-shim.h"

#include <assert.h>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <libgen.h>
#include <limits>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <wayland-client.h>
#include <xcb/composite.h>
#include <xcb/shape.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xcb/xproto.h>

#include "aura-shell-client-protocol.h"  // NOLINT(build/include_directory)
#include "drm-server-protocol.h"         // NOLINT(build/include_directory)
#include "fractional-scale-v1-client-protocol.h"  // NOLINT(build/include_directory)
#ifdef GAMEPAD_SUPPORT
#include "gaming-input-unstable-v2-client-protocol.h"  // NOLINT(build/include_directory)
#endif
#include "idle-inhibit-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "keyboard-extension-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "linux-dmabuf-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "linux-explicit-synchronization-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "pointer-constraints-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "relative-pointer-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "stylus-unstable-v2-client-protocol.h"  // NOLINT(build/include_directory)
#include "text-input-extension-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "text-input-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "viewporter-client-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-output-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-shell-client-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-shell-shim.h"             // NOLINT(build/include_directory)

#ifdef QUIRKS_SUPPORT
#include "quirks/sommelier-quirks.h"
#endif

// Check that required macro definitions exist.
#ifndef XWAYLAND_PATH
#error XWAYLAND_PATH must be defined
#endif
#ifndef XWAYLAND_GL_DRIVER_PATH
#error XWAYLAND_GL_DRIVER_PATH must be defined
#endif
#ifndef FRAME_COLOR
#error FRAME_COLOR must be defined
#endif
#ifndef DARK_FRAME_COLOR
#error DARK_FRAME_COLOR must be defined
#endif

struct sl_data_source {
  struct sl_context* ctx;
  struct wl_data_source* internal;
};

#define SEND_EVENT_MASK 0x80

#define MIN_SCALE 0.1
#define MAX_SCALE 10.0

#define MIN_DPI 72
#define MAX_DPI 9600

#define XCURSOR_SIZE_BASE 24

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define LOCK_SUFFIX ".lock"
#define LOCK_SUFFIXLEN 5

#define MIN_AURA_SHELL_VERSION 6
#define MAX_AURA_SHELL_VERSION 38

static const char STEAM_APP_CLASS_PREFIX[] = "steam_app_";

int sl_open_wayland_socket(const char* socket_name,
                           struct sockaddr_un* addr,
                           int* lock_fd,
                           int* sock_fd);

const char* net_wm_state_to_string(int i) {
  switch (i) {
    case NET_WM_STATE_REMOVE:
      return "NET_WM_STATE_REMOVE";
    case NET_WM_STATE_ADD:
      return "NET_WM_STATE_ADD";
    case NET_WM_STATE_TOGGLE:
      return "NET_WM_STATE_TOGGLE";
  }
  return "<unknown NET_WM_STATE>";
}

struct sl_sync_point* sl_sync_point_create(int fd) {
  TRACE_EVENT("sync", "sl_sync_point_create");
  struct sl_sync_point* sync_point = new sl_sync_point();
  sync_point->fd = fd;
  sync_point->sync = nullptr;

  return sync_point;
}

void sl_sync_point_destroy(struct sl_sync_point* sync_point) {
  TRACE_EVENT("sync", "sl_sync_point_destroy");
  close(sync_point->fd);
  delete sync_point;
}

static void sl_internal_xdg_shell_ping(void* data,
                                       struct xdg_wm_base* xdg_shell,
                                       uint32_t serial) {
  TRACE_EVENT("shell", "sl_internal_xdg_shell_ping");
  xdg_wm_base_pong(xdg_shell, serial);
}

static const struct xdg_wm_base_listener sl_internal_xdg_shell_listener = {
    sl_internal_xdg_shell_ping};

static void sl_adjust_window_size_for_screen_size(struct sl_window* window) {
  TRACE_EVENT("surface", "sl_adjust_window_size_for_screen_size", "id",
              window->id);
  struct sl_context* ctx = window->ctx;

  if (!sl_window_is_containerized(window)) {
    // Clamp size to screen if the window is not containerized. Containerized
    // window's size can only be modified by the X11 client.
    window->width = MIN(window->width, ctx->screen->width_in_pixels);
    window->height = MIN(window->height, ctx->screen->height_in_pixels);
  }
}

static void sl_adjust_window_position_for_screen_size(
    struct sl_window* window) {
  struct sl_context* ctx = window->ctx;

  const sl_host_output* output =
      window->paired_surface ? window->paired_surface->output.get() : nullptr;
  if (output) {
    window->x =
        output->virt_x + (output->virt_rotated_width - window->width) / 2;
    window->y = (output->virt_rotated_height - window->height) / 2;
  } else {
    // Center horizontally/vertically.
    window->x = ctx->screen->width_in_pixels / 2 - window->width / 2;
    window->y = ctx->screen->height_in_pixels / 2 - window->height / 2;
  }
}

static void sl_set_input_focus(struct sl_context* ctx,
                               struct sl_window* window) {
  if (window) {
    xcb_client_message_event_t event;
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = window->id;
    event.type = ctx->atoms[ATOM_WM_PROTOCOLS].value;
    event.data.data32[0] = ctx->atoms[ATOM_WM_TAKE_FOCUS].value;
    event.data.data32[1] = XCB_CURRENT_TIME;

    if (!window->managed)
      return;

    if (window->focus_model_take_focus) {
      xcb_send_event(ctx->connection, 0, window->id, XCB_EVENT_MASK_NO_EVENT,
                     reinterpret_cast<char*>(&event));
    }

    xcb_set_input_focus(ctx->connection, XCB_INPUT_FOCUS_NONE, window->id,
                        XCB_CURRENT_TIME);
  } else {
    xcb_set_input_focus(ctx->connection, XCB_INPUT_FOCUS_NONE, XCB_NONE,
                        XCB_CURRENT_TIME);
  }
}

void sl_restack_windows(struct sl_context* ctx, uint32_t focus_resource_id) {
  struct sl_window* sibling;
  uint32_t values[1];

  wl_list_for_each(sibling, &ctx->windows, link) {
    if (!sibling->managed)
      continue;

    // Move focus window to the top and all other windows to the bottom.
    values[0] = sibling->host_surface_id == focus_resource_id
                    ? XCB_STACK_MODE_ABOVE
                    : XCB_STACK_MODE_BELOW;
    xcb_configure_window(ctx->connection, sibling->frame_id,
                         XCB_CONFIG_WINDOW_STACK_MODE, values);
  }
}

void sl_roundtrip(struct sl_context* ctx) {
  TRACE_EVENT("other", "sl_roundtrip", "id",
              ctx->application_id != nullptr ? ctx->application_id : "<null>");
  free(xcb_get_input_focus_reply(
      ctx->connection, xcb_get_input_focus(ctx->connection), nullptr));
}

static void sl_window_set_wm_state(struct sl_window* window, int state) {
  TRACE_EVENT("surface", "sl_window_set_wm_state", "id", window->id);
  struct sl_context* ctx = window->ctx;
  uint32_t values[2];

  values[0] = state;
  values[1] = XCB_WINDOW_NONE;

  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, window->id,
                      ctx->atoms[ATOM_WM_STATE].value,
                      ctx->atoms[ATOM_WM_STATE].value, 32, 2, values);
}

static void sl_host_buffer_destroy(struct wl_client* client,
                                   struct wl_resource* resource) {
  TRACE_EVENT("surface", "sl_host_buffer_destroy");
  wl_resource_destroy(resource);
}

static const struct wl_buffer_interface sl_buffer_implementation = {
    sl_host_buffer_destroy};

static void sl_buffer_release(void* data, struct wl_buffer* buffer) {
  struct sl_host_buffer* host =
      static_cast<sl_host_buffer*>(wl_buffer_get_user_data(buffer));

  auto resource_id = host->resource ? wl_resource_get_id(host->resource) : -1;
  TRACE_EVENT("surface", "sl_buffer_release", "resource_id", resource_id);
  if (host->ctx->timing != nullptr) {
    host->ctx->timing->UpdateLastRelease(resource_id);
  }
  wl_buffer_send_release(host->resource);
}

static const struct wl_buffer_listener sl_buffer_listener = {sl_buffer_release};

static void sl_destroy_host_buffer(struct wl_resource* resource) {
  TRACE_EVENT("surface", "sl_destroy_host_buffer", "resource_id",
              resource ? wl_resource_get_id(resource) : -1);
  struct sl_host_buffer* host =
      static_cast<sl_host_buffer*>(wl_resource_get_user_data(resource));

  if (host->proxy)
    wl_buffer_destroy(host->proxy);
  if (host->shm_mmap) {
    host->shm_mmap->buffer_resource = nullptr;
    sl_mmap_unref(host->shm_mmap);
  }
  if (host->sync_point) {
    sl_sync_point_destroy(host->sync_point);
  }
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

struct sl_host_buffer* sl_create_host_buffer(struct sl_context* ctx,
                                             struct wl_client* client,
                                             uint32_t id,
                                             struct wl_buffer* proxy,
                                             int32_t width,
                                             int32_t height,
                                             bool is_drm) {
  TRACE_EVENT("surface", "sl_create_host_buffer", "id", id);
  struct sl_host_buffer* host_buffer = new sl_host_buffer();

  host_buffer->ctx = ctx;
  host_buffer->width = width;
  host_buffer->height = height;
  host_buffer->resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
  wl_resource_set_implementation(host_buffer->resource,
                                 &sl_buffer_implementation, host_buffer,
                                 sl_destroy_host_buffer);
  host_buffer->shm_mmap = nullptr;
  host_buffer->shm_format = 0;
  host_buffer->proxy = proxy;
  if (host_buffer->proxy) {
    wl_buffer_add_listener(host_buffer->proxy, &sl_buffer_listener,
                           host_buffer);
  }
  host_buffer->sync_point = nullptr;
  host_buffer->is_drm = is_drm;

  return host_buffer;
}

static void sl_internal_data_offer_destroy(struct sl_data_offer* host) {
  TRACE_EVENT("other", "sl_internal_data_offer_destroy");
  wl_data_offer_destroy(host->internal);
  wl_array_release(&host->atoms);
  wl_array_release(&host->cookies);
  delete host;
}

static void sl_set_selection(struct sl_context* ctx,
                             struct sl_data_offer* data_offer) {
  TRACE_EVENT("other", "sl_set_selection");
  if (ctx->selection_data_offer) {
    sl_internal_data_offer_destroy(ctx->selection_data_offer);
    ctx->selection_data_offer = nullptr;
  }

  if (ctx->clipboard_manager) {
    if (!data_offer) {
      if (ctx->selection_owner == ctx->selection_window)
        xcb_set_selection_owner(ctx->connection, XCB_ATOM_NONE,
                                ctx->atoms[ATOM_CLIPBOARD].value,
                                ctx->selection_timestamp);
      return;
    }

    int atoms = data_offer->cookies.size / sizeof(xcb_intern_atom_cookie_t);
    wl_array_add(&data_offer->atoms, sizeof(xcb_atom_t) * (atoms + 2));
    (reinterpret_cast<xcb_atom_t*>(data_offer->atoms.data))[0] =
        ctx->atoms[ATOM_TARGETS].value;
    (reinterpret_cast<xcb_atom_t*>(data_offer->atoms.data))[1] =
        ctx->atoms[ATOM_TIMESTAMP].value;
    for (int i = 0; i < atoms; i++) {
      xcb_intern_atom_cookie_t cookie =
          (reinterpret_cast<xcb_intern_atom_cookie_t*>(
              data_offer->cookies.data))[i];
      xcb_intern_atom_reply_t* reply =
          xcb_intern_atom_reply(ctx->connection, cookie, nullptr);
      if (reply) {
        (reinterpret_cast<xcb_atom_t*>(data_offer->atoms.data))[i + 2] =
            reply->atom;
        free(reply);
      }
    }

    xcb_set_selection_owner(ctx->connection, ctx->selection_window,
                            ctx->atoms[ATOM_CLIPBOARD].value, XCB_CURRENT_TIME);
  }

  ctx->selection_data_offer = data_offer;
}

static void sl_internal_data_offer_offer(void* data,
                                         struct wl_data_offer* data_offer,
                                         const char* type) {
  TRACE_EVENT("other", "sl_internal_data_offer_offer");
  struct sl_data_offer* host = static_cast<sl_data_offer*>(data);
  xcb_intern_atom_cookie_t* cookie = static_cast<xcb_intern_atom_cookie_t*>(
      wl_array_add(&host->cookies, sizeof(xcb_intern_atom_cookie_t)));
  *cookie = xcb_intern_atom(host->ctx->connection, 0, strlen(type), type);
}

static void sl_internal_data_offer_source_actions(
    void* data, struct wl_data_offer* data_offer, uint32_t source_actions) {
  TRACE_EVENT("other", "sl_internal_data_offer_source_actions");
}

static void sl_internal_data_offer_action(void* data,
                                          struct wl_data_offer* data_offer,
                                          uint32_t dnd_action) {
  TRACE_EVENT("other", "sl_internal_data_offer_action");
}

static const struct wl_data_offer_listener sl_internal_data_offer_listener = {
    sl_internal_data_offer_offer, sl_internal_data_offer_source_actions,
    sl_internal_data_offer_action};

static void sl_internal_data_device_data_offer(
    void* data,
    struct wl_data_device* data_device,
    struct wl_data_offer* data_offer) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_data_offer* host_data_offer = new sl_data_offer();

  host_data_offer->ctx = ctx;
  host_data_offer->internal = data_offer;
  wl_array_init(&host_data_offer->atoms);
  wl_array_init(&host_data_offer->cookies);

  wl_data_offer_add_listener(host_data_offer->internal,
                             &sl_internal_data_offer_listener, host_data_offer);
}

static void sl_internal_data_device_selection(
    void* data,
    struct wl_data_device* data_device,
    struct wl_data_offer* data_offer) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_data_offer* host_data_offer =
      data_offer
          ? static_cast<sl_data_offer*>(wl_data_offer_get_user_data(data_offer))
          : nullptr;

  sl_set_selection(ctx, host_data_offer);
}

static const struct wl_data_device_listener sl_internal_data_device_listener = {
    sl_internal_data_device_data_offer,
    /*enter=*/DoNothing,
    /*leave=*/DoNothing,
    /*motion=*/DoNothing,
    /*drop=*/DoNothing,
    sl_internal_data_device_selection};

void sl_host_seat_added(struct sl_host_seat* host) {
  struct sl_context* ctx = host->seat->ctx;

  if (ctx->default_seat)
    return;

  ctx->default_seat = host;

  // Get data device for selections.
  if (ctx->data_device_manager && ctx->data_device_manager->internal) {
    ctx->selection_data_device = wl_data_device_manager_get_data_device(
        ctx->data_device_manager->internal, host->proxy);
    wl_data_device_add_listener(ctx->selection_data_device,
                                &sl_internal_data_device_listener, ctx);
  }

#ifdef GAMEPAD_SUPPORT
  sl_gaming_seat_add_listener(ctx);
#endif
}

void sl_host_seat_removed(struct sl_host_seat* host) {
  TRACE_EVENT("other", "sl_host_seat_removed");
  if (host->seat->ctx->default_seat == host)
    host->seat->ctx->default_seat = nullptr;
}

bool sl_client_supports_interface(const sl_context* ctx,
                                  const wl_client* client,
                                  const wl_interface* interface) {
  if (ctx->client == client) {
    return true;
  }
  // Clients created for IME support only receive the minimal set of required
  // interfaces.
  const char* name = interface->name;
  return strcmp(name, "wl_seat") == 0 ||
         strcmp(name, "zwp_text_input_manager_v1") == 0 ||
         strcmp(name, "zcr_text_input_extension_v1") == 0 ||
         strcmp(name, "zcr_text_input_crostini_manager_v1") == 0 ||
         strcmp(name, "zcr_text_input_x11_v1") == 0;
}

static void sl_global_destroy(struct sl_global* global) {
  TRACE_EVENT("other", "sl_global_destroy");
  struct sl_host_registry* registry;

  wl_list_for_each(registry, &global->ctx->registries, link) {
    if (sl_client_supports_interface(global->ctx,
                                     wl_resource_get_client(registry->resource),
                                     global->interface)) {
      wl_resource_post_event(registry->resource, WL_REGISTRY_GLOBAL_REMOVE,
                             global->name);
    }
  }

  wl_list_remove(&global->link);
  free(global);
}

// Called on each "wl_registry::global" event from the host compositor,
// giving Sommelier an opportunity to bind to the new global object
// (so we can receive events or invoke requests on it), and/or forward the
// wl_registry::global event on to our clients.
void sl_registry_handler(void* data,
                         struct wl_registry* registry,
                         uint32_t id,
                         const char* interface,
                         uint32_t version) {
  struct sl_context* ctx = (struct sl_context*)data;

  TRACE_EVENT("other", "sl_registry_handler", "id", id);
  if (strcmp(interface, "wl_compositor") == 0) {
    sl_compositor_init_context(ctx, registry, id, version);
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    struct sl_subcompositor* subcompositor =
        static_cast<sl_subcompositor*>(malloc(sizeof(struct sl_subcompositor)));
    assert(subcompositor);
    subcompositor->ctx = ctx;
    subcompositor->id = id;
    assert(!ctx->subcompositor);
    ctx->subcompositor = subcompositor;
    subcompositor->host_global = sl_subcompositor_global_create(ctx);
  } else if (strcmp(interface, "wl_shm") == 0) {
    struct sl_shm* shm = static_cast<sl_shm*>(malloc(sizeof(struct sl_shm)));
    assert(shm);
    shm->ctx = ctx;
    shm->id = id;
    shm->internal = static_cast<wl_shm*>(
        wl_registry_bind(registry, id, &wl_shm_interface, 1));
    assert(!ctx->shm);
    ctx->shm = shm;
    shm->host_global = sl_shm_global_create(ctx);
  } else if (strcmp(interface, "wl_shell") == 0) {
    struct sl_shell* shell =
        static_cast<sl_shell*>(malloc(sizeof(struct sl_shell)));
    assert(shell);
    shell->ctx = ctx;
    shell->id = id;
    assert(!ctx->shell);
    ctx->shell = shell;
    shell->host_global = sl_shell_global_create(ctx);
  } else if (strcmp(interface, "wl_output") == 0) {
    struct sl_output* output =
        static_cast<sl_output*>(malloc(sizeof(struct sl_output)));
    assert(output);
    output->ctx = ctx;
    output->id = id;
    output->version = MIN(3, version);
    output->host_global = sl_output_global_create(output);
    wl_list_insert(&ctx->outputs, &output->link);
    output->host_output = nullptr;
  } else if (strcmp(interface, "wl_seat") == 0) {
    struct sl_seat* seat =
        static_cast<sl_seat*>(malloc(sizeof(struct sl_seat)));
    assert(seat);
    seat->ctx = ctx;
    seat->id = id;
    seat->version = MIN(5, version);
    seat->last_serial = 0;
    seat->stylus_tablet = nullptr;
    seat->host_global = sl_seat_global_create(seat);
    wl_list_insert(&ctx->seats, &seat->link);
  } else if (strcmp(interface, "zwp_relative_pointer_manager_v1") == 0) {
    struct sl_relative_pointer_manager* relative_pointer =
        static_cast<sl_relative_pointer_manager*>(
            malloc(sizeof(struct sl_relative_pointer_manager)));
    assert(relative_pointer);
    relative_pointer->ctx = ctx;
    relative_pointer->id = id;
    assert(!ctx->relative_pointer_manager);
    ctx->relative_pointer_manager = relative_pointer;
    relative_pointer->host_global =
        sl_relative_pointer_manager_global_create(ctx);
  } else if (strcmp(interface, "zwp_pointer_constraints_v1") == 0) {
    struct sl_pointer_constraints* pointer_constraints =
        static_cast<sl_pointer_constraints*>(
            malloc(sizeof(struct sl_pointer_constraints)));
    assert(pointer_constraints);
    pointer_constraints->ctx = ctx;
    pointer_constraints->id = id;
    assert(!ctx->pointer_constraints);
    ctx->pointer_constraints = pointer_constraints;
    pointer_constraints->host_global =
        sl_pointer_constraints_global_create(ctx);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    struct sl_data_device_manager* data_device_manager =
        static_cast<sl_data_device_manager*>(
            malloc(sizeof(struct sl_data_device_manager)));
    assert(data_device_manager);
    data_device_manager->ctx = ctx;
    data_device_manager->id = id;
    data_device_manager->version = MIN(3, version);
    data_device_manager->internal = nullptr;
    data_device_manager->host_global = nullptr;
    assert(!ctx->data_device_manager);
    ctx->data_device_manager = data_device_manager;
    if (ctx->xwayland) {
      data_device_manager->internal = static_cast<wl_data_device_manager*>(
          wl_registry_bind(registry, id, &wl_data_device_manager_interface,
                           data_device_manager->version));
    } else {
      data_device_manager->host_global =
          sl_data_device_manager_global_create(ctx);
    }
  } else if (strcmp(interface, "xdg_wm_base") == 0) {
    struct sl_xdg_shell* xdg_shell =
        static_cast<sl_xdg_shell*>(malloc(sizeof(struct sl_xdg_shell)));
    assert(xdg_shell);
    xdg_shell->ctx = ctx;
    xdg_shell->id = id;
    xdg_shell->version = MIN(SL_XDG_SHELL_MAX_VERSION, version);
    xdg_shell->internal = nullptr;
    xdg_shell->host_global = nullptr;
    assert(!ctx->xdg_shell);
    ctx->xdg_shell = xdg_shell;
    if (ctx->xwayland) {
      xdg_shell->internal = static_cast<xdg_wm_base*>(wl_registry_bind(
          registry, id, &xdg_wm_base_interface, xdg_shell->version));
      xdg_wm_base_add_listener(xdg_shell->internal,
                               &sl_internal_xdg_shell_listener, nullptr);
    } else {
      xdg_shell->host_global =
          sl_xdg_shell_global_create(ctx, xdg_shell->version);
    }
  } else if (strcmp(interface, "zaura_shell") == 0) {
    if (version >= MIN_AURA_SHELL_VERSION) {
      struct sl_aura_shell* aura_shell =
          static_cast<sl_aura_shell*>(malloc(sizeof(struct sl_aura_shell)));
      assert(aura_shell);
      aura_shell->ctx = ctx;
      aura_shell->id = id;
      aura_shell->version = MIN(MAX_AURA_SHELL_VERSION, version);
      aura_shell->host_gtk_shell_global = nullptr;
      aura_shell->internal = static_cast<zaura_shell*>(wl_registry_bind(
          registry, id, &zaura_shell_interface, aura_shell->version));
      assert(!ctx->aura_shell);
      ctx->aura_shell = aura_shell;
      aura_shell->host_gtk_shell_global = sl_gtk_shell_global_create(ctx);
    }
  } else if (strcmp(interface, "wp_viewporter") == 0) {
    struct sl_viewporter* viewporter =
        static_cast<sl_viewporter*>(malloc(sizeof(struct sl_viewporter)));
    assert(viewporter);
    viewporter->ctx = ctx;
    viewporter->id = id;
    viewporter->host_viewporter_global = nullptr;
    viewporter->internal = static_cast<wp_viewporter*>(
        wl_registry_bind(registry, id, &wp_viewporter_interface, 1));
    assert(!ctx->viewporter);
    ctx->viewporter = viewporter;
    viewporter->host_viewporter_global = sl_viewporter_global_create(ctx);
    // Allow non-integer scale.
    ctx->scale = MIN(MAX_SCALE, MAX(MIN_SCALE, ctx->desired_scale));
  } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
    struct sl_linux_dmabuf* linux_dmabuf =
        static_cast<sl_linux_dmabuf*>(malloc(sizeof(struct sl_linux_dmabuf)));
    assert(linux_dmabuf);
    linux_dmabuf->ctx = ctx;
    linux_dmabuf->id = id;
    linux_dmabuf->version = MIN(SL_LINUX_DMABUF_MAX_VERSION, version);

    linux_dmabuf->host_drm_global = sl_drm_global_create(ctx, linux_dmabuf);

    if (ctx->enable_linux_dmabuf) {
      linux_dmabuf->host_linux_dmabuf_global =
          sl_linux_dmabuf_global_create(ctx, linux_dmabuf);
    }

    if (linux_dmabuf->version >= 2) {
      linux_dmabuf->proxy_v2 = static_cast<zwp_linux_dmabuf_v1*>(
          wl_registry_bind(registry, id, &zwp_linux_dmabuf_v1_interface, 2));
    }

    ctx->linux_dmabuf = linux_dmabuf;
  } else if (strcmp(interface, "zwp_linux_explicit_synchronization_v1") == 0) {
    struct sl_linux_explicit_synchronization* linux_explicit_synchronization =
        static_cast<sl_linux_explicit_synchronization*>(
            malloc(sizeof(struct sl_linux_explicit_synchronization)));
    assert(linux_explicit_synchronization);
    linux_explicit_synchronization->ctx = ctx;
    linux_explicit_synchronization->id = id;
    linux_explicit_synchronization->internal =
        static_cast<zwp_linux_explicit_synchronization_v1*>(wl_registry_bind(
            registry, id, &zwp_linux_explicit_synchronization_v1_interface, 1));
    assert(!ctx->linux_explicit_synchronization);
    ctx->linux_explicit_synchronization = linux_explicit_synchronization;
  } else if (strcmp(interface, "zcr_keyboard_extension_v1") == 0) {
    struct sl_keyboard_extension* keyboard_extension =
        static_cast<sl_keyboard_extension*>(
            malloc(sizeof(struct sl_keyboard_extension)));
    assert(keyboard_extension);
    keyboard_extension->ctx = ctx;
    keyboard_extension->id = id;
    keyboard_extension->internal =
        static_cast<zcr_keyboard_extension_v1*>(wl_registry_bind(
            registry, id, &zcr_keyboard_extension_v1_interface, 1));
    assert(!ctx->keyboard_extension);
    ctx->keyboard_extension = keyboard_extension;
  } else if (strcmp(interface, "zwp_text_input_manager_v1") == 0) {
    struct sl_text_input_manager* text_input_manager =
        static_cast<sl_text_input_manager*>(
            malloc(sizeof(struct sl_text_input_manager)));
    assert(text_input_manager);
    text_input_manager->ctx = ctx;
    text_input_manager->id = id;
    text_input_manager->host_global =
        sl_text_input_manager_global_create(ctx, version);
    text_input_manager->host_crostini_manager_global =
        sl_text_input_crostini_manager_global_create(ctx);
    text_input_manager->host_x11_global = sl_text_input_x11_global_create(ctx);
    assert(!ctx->text_input_manager);
    ctx->text_input_manager = text_input_manager;
  } else if (strcmp(interface, "zcr_text_input_extension_v1") == 0) {
    struct sl_text_input_extension* text_input_extension =
        static_cast<sl_text_input_extension*>(
            malloc(sizeof(struct sl_text_input_extension)));
    assert(text_input_extension);
    text_input_extension->ctx = ctx;
    text_input_extension->id = id;
    text_input_extension->host_global =
        sl_text_input_extension_global_create(ctx, version);
    assert(!ctx->text_input_extension);
    ctx->text_input_extension = text_input_extension;
#ifdef GAMEPAD_SUPPORT
  } else if (strcmp(interface, "zcr_gaming_input_v2") == 0) {
    struct sl_gaming_input_manager* gaming_input_manager =
        static_cast<sl_gaming_input_manager*>(
            malloc(sizeof(struct sl_gaming_input_manager)));
    assert(gaming_input_manager);
    gaming_input_manager->ctx = ctx;
    gaming_input_manager->id = id;
    gaming_input_manager->internal = static_cast<zcr_gaming_input_v2*>(
        wl_registry_bind(registry, id, &zcr_gaming_input_v2_interface, 2));
    assert(!ctx->gaming_input_manager);
    ctx->gaming_input_manager = gaming_input_manager;
#endif
  } else if (strcmp(interface, "zcr_stylus_v2") == 0) {
    struct sl_stylus_input_manager* stylus_input_manager =
        static_cast<sl_stylus_input_manager*>(
            malloc(sizeof(struct sl_stylus_input_manager)));
    assert(stylus_input_manager);
    stylus_input_manager->ctx = ctx;
    stylus_input_manager->id = id;
    stylus_input_manager->internal = static_cast<zcr_stylus_v2*>(
        wl_registry_bind(registry, id, &zcr_stylus_v2_interface, 1));

    // Note: This does not forward the stylus-unstable-v2 protcol to the
    // clients. Instead, it exposes tablet-unstable-v2 protocol to the clients.
    // Note: This is the only user of ctx->stylus_input_manager
    stylus_input_manager->tablet_host_global =
        sl_stylus_to_tablet_manager_global_create(ctx);

    assert(!ctx->stylus_input_manager);
    ctx->stylus_input_manager = stylus_input_manager;
  } else if (strcmp(interface, "zxdg_output_manager_v1") == 0 &&
             ctx->use_direct_scale) {
    // This protocol cannot be bound unconditionally as doing so
    // causes issues under Crostini. For this reason we will only
    // bind it when we need to do so (direct mode enabled).

    struct sl_xdg_output_manager* output_manager =
        static_cast<sl_xdg_output_manager*>(
            malloc(sizeof(struct sl_xdg_output_manager)));
    assert(output_manager);

    output_manager->ctx = ctx;
    output_manager->id = id;
    output_manager->internal =
        static_cast<zxdg_output_manager_v1*>(wl_registry_bind(
            registry, id, &zxdg_output_manager_v1_interface, MIN(3, version)));
    ctx->xdg_output_manager = output_manager;
  } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) ==
             0) {
    struct sl_fractional_scale_manager* fractional_scale_manager =
        static_cast<sl_fractional_scale_manager*>(
            malloc(sizeof(struct sl_fractional_scale_manager)));
    assert(fractional_scale_manager);
    fractional_scale_manager->ctx = ctx;
    fractional_scale_manager->id = id;
    fractional_scale_manager->host_fractional_scale_manager_global = nullptr;
    fractional_scale_manager->internal =
        static_cast<wp_fractional_scale_manager_v1*>(wl_registry_bind(
            registry, id, &wp_fractional_scale_manager_v1_interface, 1));
    assert(!ctx->fractional_scale_manager);
    ctx->fractional_scale_manager = fractional_scale_manager;
    fractional_scale_manager->host_fractional_scale_manager_global =
        sl_fractional_scale_manager_global_create(ctx);
  } else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0) {
    struct sl_idle_inhibit_manager* idle_inhibit =
        static_cast<sl_idle_inhibit_manager*>(
            malloc(sizeof(struct sl_idle_inhibit_manager)));
    assert(idle_inhibit);
    idle_inhibit->ctx = ctx;
    idle_inhibit->id = id;
    assert(!ctx->idle_inhibit_manager);
    ctx->idle_inhibit_manager = idle_inhibit;
    idle_inhibit->host_global = sl_idle_inhibit_manager_global_create(ctx);
  }
}

void sl_registry_remover(void* data,
                         struct wl_registry* registry,
                         uint32_t id) {
  TRACE_EVENT("other", "sl_registry_remover");
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_output* output;
  struct sl_seat* seat;

  if (ctx->compositor && ctx->compositor->id == id) {
    sl_global_destroy(ctx->compositor->host_global);
    wl_compositor_destroy(ctx->compositor->internal);
    free(ctx->compositor);
    ctx->compositor = nullptr;
    return;
  }
  if (ctx->subcompositor && ctx->subcompositor->id == id) {
    sl_global_destroy(ctx->subcompositor->host_global);
    wl_shm_destroy(ctx->shm->internal);
    free(ctx->subcompositor);
    ctx->subcompositor = nullptr;
    return;
  }
  if (ctx->shm && ctx->shm->id == id) {
    sl_global_destroy(ctx->shm->host_global);
    free(ctx->shm);
    ctx->shm = nullptr;
    return;
  }
  if (ctx->shell && ctx->shell->id == id) {
    sl_global_destroy(ctx->shell->host_global);
    free(ctx->shell);
    ctx->shell = nullptr;
    return;
  }
  if (ctx->data_device_manager && ctx->data_device_manager->id == id) {
    if (ctx->data_device_manager->host_global)
      sl_global_destroy(ctx->data_device_manager->host_global);
    if (ctx->data_device_manager->internal)
      wl_data_device_manager_destroy(ctx->data_device_manager->internal);
    free(ctx->data_device_manager);
    ctx->data_device_manager = nullptr;
    return;
  }
  if (ctx->xdg_shell && ctx->xdg_shell->id == id) {
    if (ctx->xdg_shell->host_global)
      sl_global_destroy(ctx->xdg_shell->host_global);
    if (ctx->xdg_shell->internal)
      xdg_wm_base_destroy(ctx->xdg_shell->internal);
    free(ctx->xdg_shell);
    ctx->xdg_shell = nullptr;
    return;
  }
  if (ctx->aura_shell && ctx->aura_shell->id == id) {
    if (ctx->aura_shell->host_gtk_shell_global)
      sl_global_destroy(ctx->aura_shell->host_gtk_shell_global);
    zaura_shell_destroy(ctx->aura_shell->internal);
    free(ctx->aura_shell);
    ctx->aura_shell = nullptr;
    return;
  }
  if (ctx->viewporter && ctx->viewporter->id == id) {
    if (ctx->viewporter->host_viewporter_global)
      sl_global_destroy(ctx->viewporter->host_viewporter_global);
    wp_viewporter_destroy(ctx->viewporter->internal);
    free(ctx->viewporter);
    ctx->viewporter = nullptr;
    return;
  }
  if (ctx->linux_dmabuf && ctx->linux_dmabuf->id == id) {
    if (ctx->linux_dmabuf->host_drm_global)
      sl_global_destroy(ctx->linux_dmabuf->host_drm_global);
    if (ctx->linux_dmabuf->proxy_v2)
      zwp_linux_dmabuf_v1_destroy(ctx->linux_dmabuf->proxy_v2);
    if (ctx->linux_dmabuf->host_linux_dmabuf_global)
      sl_global_destroy(ctx->linux_dmabuf->host_linux_dmabuf_global);
    free(ctx->linux_dmabuf);
    ctx->linux_dmabuf = nullptr;
    return;
  }
  if (ctx->linux_explicit_synchronization &&
      ctx->linux_explicit_synchronization->id == id) {
    zwp_linux_explicit_synchronization_v1_destroy(
        ctx->linux_explicit_synchronization->internal);
    free(ctx->linux_explicit_synchronization);
    ctx->linux_explicit_synchronization = nullptr;
    return;
  }
  if (ctx->keyboard_extension && ctx->keyboard_extension->id == id) {
    zcr_keyboard_extension_v1_destroy(ctx->keyboard_extension->internal);
    free(ctx->keyboard_extension);
    ctx->keyboard_extension = nullptr;
    return;
  }
  if (ctx->text_input_manager && ctx->text_input_manager->id == id) {
    sl_global_destroy(ctx->text_input_manager->host_global);
    free(ctx->text_input_manager);
    ctx->text_input_manager = nullptr;
    return;
  }
  if (ctx->text_input_extension && ctx->text_input_extension->id == id) {
    sl_global_destroy(ctx->text_input_extension->host_global);
    free(ctx->text_input_extension);
    ctx->text_input_extension = nullptr;
    return;
  }
#ifdef GAMEPAD_SUPPORT
  if (ctx->gaming_input_manager && ctx->gaming_input_manager->id == id) {
    zcr_gaming_input_v2_destroy(ctx->gaming_input_manager->internal);
    free(ctx->gaming_input_manager);
    ctx->gaming_input_manager = nullptr;
    return;
  }
#endif
  if (ctx->stylus_input_manager && ctx->stylus_input_manager->id == id) {
    sl_global_destroy(ctx->stylus_input_manager->tablet_host_global);
    free(ctx->stylus_input_manager);
    ctx->stylus_input_manager = nullptr;
  }
  if (ctx->relative_pointer_manager &&
      ctx->relative_pointer_manager->id == id) {
    sl_global_destroy(ctx->relative_pointer_manager->host_global);
    free(ctx->relative_pointer_manager);
    ctx->relative_pointer_manager = nullptr;
    return;
  }
  if (ctx->pointer_constraints && ctx->pointer_constraints->id == id) {
    sl_global_destroy(ctx->pointer_constraints->host_global);
    free(ctx->pointer_constraints);
    ctx->pointer_constraints = nullptr;
    return;
  }
  if (ctx->idle_inhibit_manager && ctx->idle_inhibit_manager->id == id) {
    sl_global_destroy(ctx->idle_inhibit_manager->host_global);
    free(ctx->idle_inhibit_manager);
    ctx->idle_inhibit_manager = nullptr;
    return;
  }
  wl_list_for_each(output, &ctx->outputs, link) {
    if (output->id == id) {
      sl_global_destroy(output->host_global);
      if (output->host_output)
        wl_resource_destroy(output->host_output->resource);
      wl_list_remove(&output->link);
      free(output);
      return;
    }
  }
  wl_list_for_each(seat, &ctx->seats, link) {
    if (seat->id == id) {
      sl_global_destroy(seat->host_global);
      wl_list_remove(&seat->link);
      free(seat);
      return;
    }
  }

  // Not reached.
  assert(0);
}

const struct wl_registry_listener sl_registry_listener = {sl_registry_handler,
                                                          sl_registry_remover};

static int sl_handle_event(int fd, uint32_t mask, void* data) {
  TRACE_EVENT("other", "sl_handle_event");
  struct sl_context* ctx = (struct sl_context*)data;
  int count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
    wl_client_flush(ctx->client);
    exit(EXIT_SUCCESS);
  }

  if (mask & WL_EVENT_READABLE)
    count = wl_display_dispatch(ctx->display);
  if (mask & WL_EVENT_WRITABLE)
    wl_display_flush(ctx->display);

  if (mask == 0) {
    count = wl_display_dispatch_pending(ctx->display);
    wl_display_flush(ctx->display);
  }

  return count;
}

void sl_create_window(struct sl_context* ctx,
                      xcb_window_t id,
                      int x,
                      int y,
                      int width,
                      int height,
                      int border_width) {
  TRACE_EVENT("surface", "sl_create_window");
  sl_window* window = new sl_window(ctx, id, x, y, width, height, border_width);
  uint32_t values[1];
  values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE;
  xcb()->change_window_attributes(ctx->connection, window->id,
                                  XCB_CW_EVENT_MASK, values);

  // Also enable shape events for this window to come in if the xshape
  // flag has been enabled
  if (ctx->enable_xshape)
    xcb_shape_select_input(ctx->connection, id, 1);
}

static void sl_destroy_window(struct sl_window* window) {
  TRACE_EVENT("surface", "sl_destroy_window");
  if (window->frame_id != XCB_WINDOW_NONE)
    xcb_destroy_window(window->ctx->connection, window->frame_id);

  if (window->xdg_popup)
    xdg_popup_destroy(window->xdg_popup);
  if (window->xdg_toplevel)
    xdg_toplevel_destroy(window->xdg_toplevel);
  if (window->xdg_surface)
    xdg_surface_destroy(window->xdg_surface);
  if (window->aura_surface)
    zaura_surface_destroy(window->aura_surface);

  delete window;
}

static int sl_is_window(struct sl_window* window, xcb_window_t id) {
  if (window->id == id)
    return 1;

  if (window->frame_id != XCB_WINDOW_NONE) {
    if (window->frame_id == id)
      return 1;
  }

  return 0;
}

struct sl_window* sl_lookup_window(struct sl_context* ctx, xcb_window_t id) {
  struct sl_window* window;

  wl_list_for_each(window, &ctx->windows, link) {
    if (sl_is_window(window, id))
      return window;
  }
  wl_list_for_each(window, &ctx->unpaired_windows, link) {
    if (sl_is_window(window, id))
      return window;
  }
  return nullptr;
}

int sl_is_our_window(struct sl_context* ctx, xcb_window_t id) {
  const xcb_setup_t* setup = xcb_get_setup(ctx->connection);

  return (id & ~setup->resource_id_mask) == setup->resource_id_base;
}

static void sl_handle_create_notify(struct sl_context* ctx,
                                    xcb_create_notify_event_t* event) {
  if (sl_is_our_window(ctx, event->window))
    return;

  sl_create_window(ctx, event->window, event->x, event->y, event->width,
                   event->height, event->border_width);
}

void sl_handle_destroy_notify(struct sl_context* ctx,
                              xcb_destroy_notify_event_t* event) {
  struct sl_window* window;

  if (sl_is_our_window(ctx, event->window))
    return;

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  sl_destroy_window(window);
}

void sl_handle_reparent_notify(struct sl_context* ctx,
                               xcb_reparent_notify_event_t* event) {
  struct sl_window* window;

  if (event->parent == ctx->screen->root) {
    int width = 1;
    int height = 1;
    int border_width = 0;

    // Return early if window is already tracked. This happens when we
    // reparent an unampped window back to the root window.
    window = sl_lookup_window(ctx, event->window);
    if (window)
      return;

    xcb_get_geometry_reply_t* geometry_reply = xcb_get_geometry_reply(
        ctx->connection, xcb_get_geometry(ctx->connection, event->window),
        nullptr);

    if (geometry_reply) {
      width = geometry_reply->width;
      height = geometry_reply->height;
      border_width = geometry_reply->border_width;
      free(geometry_reply);
    }
    sl_create_window(ctx, event->window, event->x, event->y, width, height,
                     border_width);
    return;
  }

  if (sl_is_our_window(ctx, event->parent))
    return;

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  sl_destroy_window(window);
}

static const char* sl_decode_wm_class(struct sl_window* window,
                                      xcb_get_property_reply_t* reply) {
  // WM_CLASS property contains two consecutive null-terminated strings.
  // These specify the Instance and Class names. If a global app ID is
  // not set then use Class name for app ID.
  const char* value = static_cast<char*>(xcb()->get_property_value(reply));
  int value_length = xcb()->get_property_value_length(reply);
  int instance_length = strnlen(value, value_length);
  if (value_length > instance_length) {
    window->clazz = strndup(value + instance_length + 1,
                            value_length - instance_length - 1);

    if (!window->steam_game_id) {
      // If there's no known Steam Game ID for this window,
      // attempt to parse one from the class name.
      if (strncmp(window->clazz, STEAM_APP_CLASS_PREFIX,
                  strlen(STEAM_APP_CLASS_PREFIX)) == 0) {
        // atoi() returns 0 on error, in which case steam_game_id
        // simply remains effectively unset.
        window->steam_game_id =
            atoi(window->clazz + strlen(STEAM_APP_CLASS_PREFIX));
      }
    }
    return window->clazz;
  }
  return nullptr;
}

static void sl_set_application_id_from_atom(struct sl_context* ctx,
                                            struct sl_window* window,
                                            xcb_get_property_reply_t* reply) {
  if (reply->type == XCB_ATOM_CARDINAL) {
    uint32_t value = *static_cast<uint32_t*>(xcb()->get_property_value(reply));
    window->app_id_property = std::to_string(value);
  }
}

static void on_steam_game_id_updated(struct sl_context* ctx,
                                     struct sl_window* window,
                                     xcb_get_property_reply_t* reply) {
  if (reply->type == XCB_ATOM_CARDINAL) {
    window->steam_game_id =
        *static_cast<uint32_t*>(xcb()->get_property_value(reply));
  } else {
    window->steam_game_id = 0;
  }
}

void sl_handle_map_request(struct sl_context* ctx,
                           xcb_map_request_event_t* event) {
  TRACE_EVENT("shm", "sl_handle_map_request", [&](perfetto::EventContext p) {
    perfetto_annotate_window(ctx, p, "window", event->window);
  });
  struct sl_window* window = sl_lookup_window(ctx, event->window);
  struct {
    int type;
    xcb_atom_t atom;
  } properties[] = {
      {PROPERTY_WM_NAME, XCB_ATOM_WM_NAME},
      {PROPERTY_NET_WM_NAME, ctx->atoms[ATOM_NET_WM_NAME].value},
      {PROPERTY_WM_CLASS, XCB_ATOM_WM_CLASS},
      {PROPERTY_WM_TRANSIENT_FOR, XCB_ATOM_WM_TRANSIENT_FOR},
      {PROPERTY_WM_NORMAL_HINTS, XCB_ATOM_WM_NORMAL_HINTS},
      {PROPERTY_WM_CLIENT_LEADER, ctx->atoms[ATOM_WM_CLIENT_LEADER].value},
      {PROPERTY_WM_PROTOCOLS, ctx->atoms[ATOM_WM_PROTOCOLS].value},
      {PROPERTY_MOTIF_WM_HINTS, ctx->atoms[ATOM_MOTIF_WM_HINTS].value},
      {PROPERTY_NET_STARTUP_ID, ctx->atoms[ATOM_NET_STARTUP_ID].value},
      {PROPERTY_NET_WM_STATE, ctx->atoms[ATOM_NET_WM_STATE].value},
      {PROPERTY_GTK_THEME_VARIANT, ctx->atoms[ATOM_GTK_THEME_VARIANT].value},
      {PROPERTY_XWAYLAND_RANDR_EMU_MONITOR_RECTS,
       ctx->atoms[ATOM_XWAYLAND_RANDR_EMU_MONITOR_RECTS].value},
      {PROPERTY_STEAM_GAME, ctx->atoms[ATOM_STEAM_GAME].value},
      {PROPERTY_SPECIFIED_FOR_APP_ID, ctx->application_id_property_atom},
      {PROPERTY_NET_WM_WINDOW_TYPE, ctx->atoms[ATOM_NET_WM_WINDOW_TYPE].value},
      {PROPERTY_NET_WM_PID, ctx->atoms[ATOM_NET_WM_PID].value},
  };
  xcb_get_geometry_cookie_t geometry_cookie;
  xcb_get_property_cookie_t property_cookies[ARRAY_SIZE(properties)];
  struct sl_wm_size_hints size_hints = {0};
  struct sl_mwm_hints mwm_hints = {0};
  bool maximize_h = false, maximize_v = false, fullscreen = false;
  uint32_t values[5];

  if (!window)
    return;

  if (sl_is_our_window(ctx, event->window))
    return;

  window->managed = 1;
  if (window->frame_id == XCB_WINDOW_NONE)
    geometry_cookie = xcb()->get_geometry(ctx->connection, window->id);

  for (unsigned i = 0; i < ARRAY_SIZE(properties); ++i) {
    property_cookies[i] =
        xcb()->get_property(ctx->connection, 0, window->id, properties[i].atom,
                            XCB_ATOM_ANY, 0, 2048);
  }

  if (window->frame_id == XCB_WINDOW_NONE) {
    xcb_get_geometry_reply_t* geometry_reply =
        xcb()->get_geometry_reply(ctx->connection, geometry_cookie, nullptr);
    if (geometry_reply) {
      window->x = geometry_reply->x;
      window->y = geometry_reply->y;
      window->width = geometry_reply->width;
      window->height = geometry_reply->height;
      window->depth = geometry_reply->depth;
      free(geometry_reply);
    }
  }

  free(window->name);
  window->name = nullptr;
  free(window->clazz);
  window->clazz = nullptr;
  free(window->startup_id);
  window->startup_id = nullptr;
  window->transient_for = XCB_WINDOW_NONE;
  window->client_leader = XCB_WINDOW_NONE;
  window->decorated = 1;
  window->size_flags = 0;
  window->dark_frame = 0;

  for (unsigned i = 0; i < ARRAY_SIZE(properties); ++i) {
    xcb_get_property_reply_t* reply = xcb()->get_property_reply(
        ctx->connection, property_cookies[i], nullptr);

    if (!reply)
      continue;

    if (reply->type == XCB_ATOM_NONE) {
      free(reply);
      continue;
    }

    const char* value = nullptr;
    int value_int = std::numeric_limits<int>::max();
    xcb_atom_t* property_values = nullptr;

    switch (properties[i].type) {
      case PROPERTY_WM_NAME:
        // WM_NAME is only used for the window name if _NET_WM_NAME is not
        // present or not yet processed, thus window->name is null.
        if (!window->has_net_wm_name) {
          window->name =
              strndup(static_cast<char*>(xcb()->get_property_value(reply)),
                      xcb()->get_property_value_length(reply));
          value = window->name;
        }
        break;
      case PROPERTY_NET_WM_NAME:
        // _NET_WM_NAME is the preferred window name property. Remove any
        // previously set window name.
        if (window->name) {
          free(window->name);
          window->name = nullptr;
        }
        window->has_net_wm_name = true;
        window->name =
            strndup(static_cast<char*>(xcb()->get_property_value(reply)),
                    xcb()->get_property_value_length(reply));
        value = window->name;
        break;
      case PROPERTY_WM_CLASS:
        value = sl_decode_wm_class(window, reply);
        if (!value)
          value = "<invalid>";
        break;
      case PROPERTY_WM_TRANSIENT_FOR:
        if (xcb()->get_property_value_length(reply) >= 4) {
          window->transient_for =
              *(reinterpret_cast<uint32_t*>(xcb()->get_property_value(reply)));
          value_int = window->transient_for;
        }
        break;
      case PROPERTY_WM_NORMAL_HINTS:
        if (xcb()->get_property_value_length(reply) >=
            static_cast<int>(sizeof(size_hints)))
          memcpy(&size_hints, xcb()->get_property_value(reply),
                 sizeof(size_hints));
        break;
      case PROPERTY_WM_CLIENT_LEADER:
        if (xcb()->get_property_value_length(reply) >= 4) {
          window->client_leader =
              *(reinterpret_cast<uint32_t*>(xcb()->get_property_value(reply)));
          value_int = window->client_leader;
        }
        break;
      case PROPERTY_WM_PROTOCOLS:
        property_values =
            static_cast<xcb_atom_t*>(xcb()->get_property_value(reply));
        for (unsigned j = 0;
             j < xcb()->get_property_value_length(reply) / sizeof(xcb_atom_t);
             ++j) {
          if (property_values[j] == ctx->atoms[ATOM_WM_TAKE_FOCUS].value) {
            window->focus_model_take_focus = 1;
            value = "ATOM_WM_TAKE_FOCUS";
          }
        }
        break;
      case PROPERTY_MOTIF_WM_HINTS:
        if (xcb()->get_property_value_length(reply) >=
            static_cast<int>(sizeof(mwm_hints)))
          memcpy(&mwm_hints, xcb()->get_property_value(reply),
                 sizeof(mwm_hints));
        break;
      case PROPERTY_NET_STARTUP_ID:
        window->startup_id =
            strndup(static_cast<char*>(xcb()->get_property_value(reply)),
                    xcb()->get_property_value_length(reply));
        value = window->startup_id;
        break;
      case PROPERTY_NET_WM_STATE:
        property_values =
            static_cast<xcb_atom_t*>(xcb()->get_property_value(reply));
        for (unsigned j = 0;
             j < xcb()->get_property_value_length(reply) / sizeof(xcb_atom_t);
             ++j) {
          if (property_values[j] ==
              ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ].value) {
            maximize_h = true;
          } else if (property_values[j] ==
                     ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT].value) {
            maximize_v = true;
          } else if (property_values[j] ==
                     ctx->atoms[ATOM_NET_WM_STATE_FULLSCREEN].value) {
            fullscreen = true;
          }
        }
        // Neither wayland not CrOS support 1D maximizing, so sommelier will
        // only consider a window maximized if both dimensions are. This
        // behaviour is consistent with sl_handle_client_message().
        window->maximized = maximize_h && maximize_v;
        window->fullscreen = fullscreen;
        LOG(VERBOSE) << window << " fullscreen=" << fullscreen;
        if (window->maximized) {
          if (window->fullscreen) {
            value =
                "_NET_WM_STATE_FULLSCREEN, _NET_WM_STATE_MAXIMIZED_VERT && "
                "HORZ";
          } else {
            value = "_NET_WM_STATE_MAXIMIZED_VERT && HORZ";
          }
        } else if (window->fullscreen) {
          value = "_NET_WM_STATE_FULLSCREEN";
        }
        break;
      case PROPERTY_GTK_THEME_VARIANT:
        if (xcb()->get_property_value_length(reply) >= 4)
          window->dark_frame = !strcmp(
              static_cast<char*>(xcb()->get_property_value(reply)), "dark");
        break;
      case PROPERTY_STEAM_GAME:
        on_steam_game_id_updated(ctx, window, reply);
        break;
      case PROPERTY_SPECIFIED_FOR_APP_ID:
        sl_set_application_id_from_atom(ctx, window, reply);
        value = window->app_id_property.c_str();
        break;
      case PROPERTY_NET_WM_WINDOW_TYPE:
        property_values =
            static_cast<xcb_atom_t*>(xcb()->get_property_value(reply));
        if (xcb()->get_property_value_length(reply) == sizeof(xcb_atom_t)) {
          window->type = property_values[0];
        }
        break;
      case PROPERTY_NET_WM_PID:
        property_values =
            static_cast<xcb_atom_t*>(xcb()->get_property_value(reply));
        if (xcb()->get_property_value_length(reply) == sizeof(xcb_atom_t)) {
          window->pid = property_values[0];
          // Double check if the window should be containerized
          sl_window_update_should_be_containerized_from_pid(window);
        }
        break;
      default:
        break;
    }

    TRACE_EVENT("x11wm", "XCB_MAP_REQUEST: X property",
                [&](perfetto::EventContext p) {
                  perfetto_annotate_atom(ctx, p, "name", properties[i].atom);

                  if (value_int != std::numeric_limits<int>::max()) {
                    auto* dbg = p.event()->add_debug_annotations();
                    dbg->set_name("int value");
                    dbg->set_int_value(value_int);
                  }
                  if (value) {
                    auto* dbg = p.event()->add_debug_annotations();
                    dbg->set_name("str value");
                    dbg->set_string_value(value, strlen(value));
                  }

                  switch (properties[i].type) {
                    case PROPERTY_WM_NORMAL_HINTS:
                      perfetto_annotate_size_hints(p, size_hints);
                      break;
                    case PROPERTY_XWAYLAND_RANDR_EMU_MONITOR_RECTS:
                      perfetto_annotate_cardinal_list(p, "value", reply);
                      break;
                  }
                });
#ifndef PERFETTO_TRACING
    UNUSED(value_int);
#endif

    free(reply);
  }

  if (mwm_hints.flags & MWM_HINTS_DECORATIONS) {
    if (mwm_hints.decorations & MWM_DECOR_ALL)
      window->decorated = ~mwm_hints.decorations & MWM_DECOR_TITLE;
    else
      window->decorated = mwm_hints.decorations & MWM_DECOR_TITLE;
  }

  // Allow user/program controlled position for transients,
  // and other client-positioned windows.
  if (window->transient_for || sl_window_is_client_positioned(window))
    window->size_flags |= size_hints.flags & (US_POSITION | P_POSITION);

  // If startup ID is not set, then try the client leader window.
  if (!window->startup_id && window->client_leader) {
    xcb_get_property_reply_t* reply = xcb()->get_property_reply(
        ctx->connection,
        xcb()->get_property(ctx->connection, 0, window->client_leader,
                            ctx->atoms[ATOM_NET_STARTUP_ID].value, XCB_ATOM_ANY,
                            0, 2048),
        nullptr);
    if (reply) {
      if (reply->type != XCB_ATOM_NONE) {
        window->startup_id =
            strndup(static_cast<char*>(xcb()->get_property_value(reply)),
                    xcb()->get_property_value_length(reply));
      }
      free(reply);
    }
  }

  window->size_flags |= size_hints.flags & (P_MIN_SIZE | P_MAX_SIZE);
  if (window->size_flags & P_MIN_SIZE) {
    window->min_width = size_hints.min_width;
    window->min_height = size_hints.min_height;
    LOG(VERBOSE) << window << " min=" << window->min_width << "x"
                 << window->min_height;
  }
  if (window->size_flags & P_MAX_SIZE) {
    if (size_hints.max_width < INT_MAX) {
      window->max_width = size_hints.max_width;
    } else {
      window->max_width = 0;
    }
    if (size_hints.max_height < INT_MAX) {
      window->max_height = size_hints.max_height;
    } else {
      window->max_height = 0;
    }
    LOG(VERBOSE) << window << " max=" << window->max_width << "x"
                 << window->max_height;
  }

  window->border_width = 0;
  sl_adjust_window_size_for_screen_size(window);
  if (!(window->size_flags & (US_POSITION | P_POSITION)))
    sl_adjust_window_position_for_screen_size(window);

  sl_window_get_width_height(window, &values[0], &values[1]);
  values[2] = 0;
  xcb()->configure_window(ctx->connection, window->id,
                          XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                              XCB_CONFIG_WINDOW_BORDER_WIDTH,
                          values);
  // This needs to match the frame extents of the X11 frame window used
  // for reparenting or applications tend to be confused. The actual window
  // frame size used by the host compositor can be different.
  values[0] = 0;
  values[1] = 0;
  values[2] = 0;
  values[3] = 0;
  xcb()->change_property(ctx->connection, XCB_PROP_MODE_REPLACE, window->id,
                         ctx->atoms[ATOM_NET_FRAME_EXTENTS].value,
                         XCB_ATOM_CARDINAL, 32, 4, values);

  // Remove weird gravities.
  values[0] = XCB_GRAVITY_NORTH_WEST;
  xcb()->change_window_attributes(ctx->connection, window->id,
                                  XCB_CW_WIN_GRAVITY, values);

  if (window->frame_id == XCB_WINDOW_NONE) {
    int depth = window->depth ? window->depth : ctx->screen->root_depth;

    values[0] = ctx->screen->black_pixel;
    values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
    values[2] = ctx->colormaps[depth];

    window->frame_id = xcb()->generate_id(ctx->connection);
    xcb()->create_window(
        ctx->connection, depth, window->frame_id, ctx->screen->root, window->x,
        window->y, window->width, window->height, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, ctx->visual_ids[depth],
        XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, values);
    values[0] = XCB_STACK_MODE_BELOW;
    xcb()->configure_window(ctx->connection, window->frame_id,
                            XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb()->reparent_window(ctx->connection, window->id, window->frame_id, 0, 0);
  } else {
    // Populate values[0~3] with x,y,w,h
    sl_window_get_x_y(window, &values[0], &values[1]);
    sl_window_get_width_height(window, &values[2], &values[3]);
    values[4] = XCB_STACK_MODE_BELOW;
    xcb()->configure_window(
        ctx->connection, window->frame_id,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_STACK_MODE,
        values);
  }

  sl_window_set_wm_state(window, WM_STATE_NORMAL);
  sl_send_configure_notify(window);

  xcb()->map_window(ctx->connection, window->id);
  xcb()->map_window(ctx->connection, window->frame_id);
}

static void sl_handle_map_notify(struct sl_context* ctx,
                                 xcb_map_notify_event_t* event) {}

void sl_handle_unmap_notify(struct sl_context* ctx,
                            xcb_unmap_notify_event_t* event) {
  struct sl_window* window;

  if (sl_is_our_window(ctx, event->window))
    return;

  if (event->response_type & SEND_EVENT_MASK)
    return;

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  if (ctx->host_focus_window == window) {
    ctx->host_focus_window = nullptr;
    ctx->needs_set_input_focus = 1;
  }

  if (window->host_surface_id) {
    window->host_surface_id = 0;
    sl_window_update(window);
  }

  sl_window_set_wm_state(window, WM_STATE_WITHDRAWN);

  // Reparent window and destroy frame if it exists.
  if (window->frame_id != XCB_WINDOW_NONE) {
    xcb_reparent_window(ctx->connection, window->id, ctx->screen->root,
                        window->x, window->y);
    xcb_destroy_window(ctx->connection, window->frame_id);
    window->frame_id = XCB_WINDOW_NONE;
  }

  // Reset properties to unmanaged state in case the window transitions to
  // an override-redirect window.
  window->managed = 0;
  window->decorated = 0;
  window->size_flags = P_POSITION;
}

void sl_handle_configure_request(struct sl_context* ctx,
                                 xcb_configure_request_event_t* event) {
  struct sl_window* window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;
  int width = window->width;
  int height = window->height;
  uint32_t values[7];

  if (sl_is_our_window(ctx, event->window))
    return;

  if (!window->managed) {
    int i = 0;

    if (event->value_mask & XCB_CONFIG_WINDOW_X)
      values[i++] = event->x;
    if (event->value_mask & XCB_CONFIG_WINDOW_Y)
      values[i++] = event->y;
    if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH)
      values[i++] = event->width;
    if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
      values[i++] = event->height;
    if (event->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
      values[i++] = event->border_width;
    if (event->value_mask & XCB_CONFIG_WINDOW_SIBLING)
      values[i++] = event->sibling;
    if (event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
      values[i++] = event->stack_mode;

    xcb_configure_window(ctx->connection, window->id, event->value_mask,
                         values);
    return;
  }

  // Ack configure events as satisfying request removes the guarantee
  // that matching contents will arrive.
  if (window->xdg_toplevel) {
    if (window->pending_config.serial) {
      xdg_surface_ack_configure(window->xdg_surface,
                                window->pending_config.serial);
      window->pending_config.serial = 0;
      window->pending_config.mask = 0;
      window->pending_config.states_length = 0;
    }
    if (window->next_config.serial) {
      xdg_surface_ack_configure(window->xdg_surface,
                                window->next_config.serial);
      window->next_config.serial = 0;
      window->next_config.mask = 0;
      window->next_config.states_length = 0;
    }
  }

  if (event->value_mask & XCB_CONFIG_WINDOW_X)
    window->x = event->x;
  if (event->value_mask & XCB_CONFIG_WINDOW_Y)
    window->y = event->y;

  if (window->allow_resize) {
    if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH)
      window->width = event->width;
    if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
      window->height = event->height;
  }

  sl_adjust_window_size_for_screen_size(window);
  if (window->size_flags & (US_POSITION | P_POSITION))
    sl_window_update(window);
  else
    sl_adjust_window_position_for_screen_size(window);

  // Populate values[0~3] with x,y,w,h
  sl_window_get_x_y(window, &values[0], &values[1]);
  sl_window_get_width_height(window, &values[2], &values[3]);
  values[4] = 0;
  xcb_configure_window(ctx->connection, window->frame_id,
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                           XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                       values);

  // We need to send a synthetic configure notify if:
  // - Not changing the size, location, border width.
  // - Moving the window without resizing it or changing its border width.
  if (width != window->width || height != window->height ||
      window->border_width) {
    xcb_configure_window(ctx->connection, window->id,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         &values[2]);
    window->border_width = 0;
  } else {
    sl_send_configure_notify(window);
  }

  // If client requested a location change, forward that to the host.
  //
  // This is handled as a special case because there's no existing way
  // to send location changes, and it's done only upon ConfigureRequest
  // since most of the time the host has authority over window position
  // (important so that window dragging works properly, for example).
  //
  // This implies sending width and height as well, only because we
  // don't have a way to omit them from the request.
  if (event->value_mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)) {
    sl_toplevel_send_window_bounds_to_host(window);
  }
}

static void sl_handle_configure_notify(struct sl_context* ctx,
                                       xcb_configure_notify_event_t* event) {
  struct sl_window* window;

  if (sl_is_our_window(ctx, event->window))
    return;

  if (event->window == ctx->screen->root) {
    xcb_get_geometry_reply_t* geometry_reply = xcb_get_geometry_reply(
        ctx->connection, xcb_get_geometry(ctx->connection, event->window),
        nullptr);
    int width = ctx->screen->width_in_pixels;
    int height = ctx->screen->height_in_pixels;

    if (geometry_reply) {
      width = geometry_reply->width;
      height = geometry_reply->height;
      free(geometry_reply);
    }

    if (width == ctx->screen->width_in_pixels ||
        height == ctx->screen->height_in_pixels) {
      return;
    }

    ctx->screen->width_in_pixels = width;
    ctx->screen->height_in_pixels = height;

    // Re-center managed windows.
    wl_list_for_each(window, &ctx->windows, link) {
      int x, y;

      if (window->size_flags & (US_POSITION | P_POSITION))
        continue;

      x = window->x;
      y = window->y;
      sl_adjust_window_position_for_screen_size(window);
      if (window->x != x || window->y != y) {
        uint32_t values[2];

        values[0] = window->x;
        values[1] = window->y;
        xcb_configure_window(ctx->connection, window->frame_id,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
        sl_send_configure_notify(window);
      }
    }
    return;
  }

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  if (window->managed)
    return;

  window->width = event->width;
  window->height = event->height;
  window->border_width = event->border_width;
  if (event->x != window->x || event->y != window->y) {
    window->x = event->x;
    window->y = event->y;
    sl_window_update(window);
  }
}

static uint32_t sl_resize_edge(int net_wm_moveresize_size) {
  switch (net_wm_moveresize_size) {
    case NET_WM_MOVERESIZE_SIZE_TOPLEFT:
      return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
    case NET_WM_MOVERESIZE_SIZE_TOP:
      return XDG_TOPLEVEL_RESIZE_EDGE_TOP;
    case NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
      return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
    case NET_WM_MOVERESIZE_SIZE_RIGHT:
      return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
    case NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
      return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
    case NET_WM_MOVERESIZE_SIZE_BOTTOM:
      return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
    case NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
      return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
    case NET_WM_MOVERESIZE_SIZE_LEFT:
      return XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
    default:
      return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
  }
}

static void sl_request_attention(struct sl_context* ctx,
                                 struct sl_window* window,
                                 bool is_strong_request) {
  if (!window->aura_surface ||
      ctx->aura_shell->version < ZAURA_SURFACE_DRAW_ATTENTION_SINCE_VERSION)
    return;
  if (is_strong_request) {
    zaura_surface_activate(window->aura_surface);
  } else {
    zaura_surface_draw_attention(window->aura_surface);
  }
}

void sl_handle_client_message(struct sl_context* ctx,
                              xcb_client_message_event_t* event) {
  TRACE_EVENT("x11wm", "XCB_CLIENT_MESSAGE", [&](perfetto::EventContext p) {
    perfetto_annotate_atom(ctx, p, "event->type", event->type);
    perfetto_annotate_window(ctx, p, "event->window", event->window);
  });
  if (event->type == ctx->atoms[ATOM_WL_SURFACE_ID].value) {
    struct sl_window *window, *unpaired_window = nullptr;

    wl_list_for_each(window, &ctx->unpaired_windows, link) {
      if (sl_is_window(window, event->window)) {
        unpaired_window = window;
        break;
      }
    }

    if (unpaired_window) {
      unpaired_window->host_surface_id = event->data.data32[0];
      sl_window_update(unpaired_window);
    }
  } else if (event->type == ctx->atoms[ATOM_NET_ACTIVE_WINDOW].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (window)
      sl_request_attention(ctx, window, /*is_strong_request=*/true);
  } else if (event->type == ctx->atoms[ATOM_NET_WM_MOVERESIZE].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);

    if (window && window->xdg_toplevel) {
      struct sl_host_seat* seat = window->ctx->default_seat;

      if (!seat)
        return;

      if (event->data.data32[2] == NET_WM_MOVERESIZE_MOVE) {
        xdg_toplevel_move(window->xdg_toplevel, seat->proxy,
                          seat->seat->last_serial);
      } else {
        uint32_t edge = sl_resize_edge(event->data.data32[2]);

        if (edge == XDG_TOPLEVEL_RESIZE_EDGE_NONE)
          return;

        xdg_toplevel_resize(window->xdg_toplevel, seat->proxy,
                            seat->seat->last_serial, edge);
      }
    }
  } else if (event->type == ctx->atoms[ATOM_NET_WM_STATE].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);

    if (window) {
      int changed[ATOM_LAST + 1];
      uint32_t action = event->data.data32[0];
      unsigned i;

      for (i = 0; i < ARRAY_SIZE(ctx->atoms); ++i) {
        changed[i] = event->data.data32[1] == ctx->atoms[i].value ||
                     event->data.data32[2] == ctx->atoms[i].value;
      }

      if (changed[ATOM_NET_WM_STATE_FULLSCREEN]) {
        TRACE_EVENT("x11wm", "XCB_CLIENT_MESSAGE: ATOM_NET_WM_STATE_FULLSCREEN",
                    "action", net_wm_state_to_string(action), "window->name",
                    window->name);
        if (action == NET_WM_STATE_ADD) {
          // Ignore additional calls to change to fullscreen
          // if X11 state is already fullscreen. Prevents issues when changing
          // focus back to game and Exo/X11 state differ.
          if (!window->fullscreen) {
            window->fullscreen = 1;
            LOG(VERBOSE) << window << " fullscreen=1";
            if (window->xdg_toplevel && !window->iconified) {
              xdg_toplevel_set_fullscreen(window->xdg_toplevel, nullptr);
            } else {
              window->pending_fullscreen_change = true;
            }
          }
        } else if (action == NET_WM_STATE_REMOVE) {
          // Ignore additional calls to change to windowed
          // if X11 state is already windowed. Prevents issues when changing
          // focus back to game and Exo/X11 state differ.
          if (window->fullscreen) {
            // Preemptively ask Exo to remove fullscreen then check during
            // surface commit (ie. after all states aggregated).
            window->maybe_promote_to_fullscreen = true;

            window->fullscreen = 0;
            LOG(VERBOSE) << window << " fullscreen=0";
            if (window->xdg_toplevel && !window->iconified) {
              xdg_toplevel_unset_fullscreen(window->xdg_toplevel);
            } else {
              window->pending_fullscreen_change = true;
            }
          }
        }
      }

      if (changed[ATOM_NET_WM_STATE_MAXIMIZED_VERT] &&
          changed[ATOM_NET_WM_STATE_MAXIMIZED_HORZ]) {
        TRACE_EVENT(
            "x11wm",
            "XCB_CLIENT_MESSAGE: ATOM_NET_WM_STATE_MAXIMIZED_VERT && HORZ",
            "action", net_wm_state_to_string(action), "window->name",
            window->name);
        if (action == NET_WM_STATE_ADD) {
          window->maximized = 1;
          if (window->xdg_toplevel && !window->iconified) {
            xdg_toplevel_set_maximized(window->xdg_toplevel);
          } else {
            window->pending_maximized_change = true;
          }
        } else if (action == NET_WM_STATE_REMOVE) {
          window->maximized = 0;
          if (window->xdg_toplevel && !window->iconified) {
            xdg_toplevel_unset_maximized(window->xdg_toplevel);
          } else {
            window->pending_maximized_change = true;
          }
        }
      }
    }
  } else if (event->type == ctx->atoms[ATOM_WM_CHANGE_STATE].value &&
             event->data.data32[0] == WM_STATE_ICONIC) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    TRACE_EVENT("x11wm", "XCB_CLIENT_MESSAGE: WM_STATE_ICONIC (minimize)",
                "window->name", window ? window->name : "<unknown>");
    if (window && window->xdg_toplevel) {
      xdg_toplevel_set_minimized(window->xdg_toplevel);
#ifdef QUIRKS_SUPPORT
      // TODO(b/333819234): Migrate FEATURE_BLACK_SCREEN_FIX to quirk condition
      // `runtime: Proton`.
      if (window->ctx->quirks.IsEnabled(window,
                                        quirks::FEATURE_BLACK_SCREEN_FIX)) {
        // Workaround for some borealis apps showing a black screen after losing
        // focus from fullscreen.
        // When a window is iconified, it should be unmapped. To return it back
        // to a visible state, it should be remapped. However sommelier does not
        // do this. Therefore we are sending a synthetic unmap then map notify
        // so that the app is rendered again.
        xcb_unmap_notify_event_t unmap_event = {
            .response_type = XCB_UNMAP_NOTIFY,
            .pad0 = 0,
            .event = window->id,
            .window = window->id,
            .from_configure = 0};

        xcb_send_event(ctx->connection, 0, window->id,
                       XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                       reinterpret_cast<char*>(&unmap_event));
        sl_send_configure_notify(window);

        sl_window_set_wm_state(window, WM_STATE_ICONIC);
        sl_send_configure_notify(window);

        // When we are iconified we want to suppress any calls that deiconify
        // the window as it should in theory be unmapped.
        window->iconified = 1;
      }
#endif  // QUIRKS_SUPPORT
    }
  }
}

void sl_handle_focus_in(struct sl_context* ctx, xcb_focus_in_event_t* event) {
  struct sl_window* window = sl_lookup_window(ctx, event->event);
  if (window && window->transient_for != XCB_WINDOW_NONE) {
    // Set our parent now as it might not have been set properly when the
    // window was realized.
    struct sl_window* parent = sl_lookup_window(ctx, window->transient_for);
    if (parent && parent->xdg_toplevel && window->xdg_toplevel)
      xdg_toplevel_set_parent(window->xdg_toplevel, parent->xdg_toplevel);
  }
  if (window) {
    if (window->xdg_toplevel && window->pending_fullscreen_change) {
      if (window->fullscreen) {
        xdg_toplevel_set_fullscreen(window->xdg_toplevel, nullptr);
      } else {
        xdg_toplevel_unset_fullscreen(window->xdg_toplevel);
      }

      window->pending_fullscreen_change = false;
    }
    if (window->xdg_toplevel && window->pending_maximized_change) {
      if (window->maximized) {
        xdg_toplevel_set_maximized(window->xdg_toplevel);
      } else {
        xdg_toplevel_unset_maximized(window->xdg_toplevel);
      }
      window->pending_maximized_change = false;
    }
    if (window->iconified) {
      // Remap the window that was iconified upon gaining focus.
      xcb_map_notify_event_t map_event = {.response_type = XCB_MAP_NOTIFY,
                                          .pad0 = 0,
                                          .event = window->id,
                                          .window = window->id,
                                          .override_redirect = 0};

      xcb_send_event(window->ctx->connection, 0, window->id,
                     XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                     reinterpret_cast<char*>(&map_event));
      sl_send_configure_notify(window);

      sl_window_set_wm_state(window, WM_STATE_NORMAL);
      sl_send_configure_notify(window);

      xcb_flush(window->ctx->connection);

      window->iconified = 0;
    }
  }
}

static void sl_handle_focus_out(struct sl_context* ctx,
                                xcb_focus_out_event_t* event) {}

int sl_begin_data_source_send(struct sl_context* ctx,
                              int fd,
                              xcb_intern_atom_cookie_t cookie,
                              struct sl_data_source* data_source) {
  xcb_intern_atom_reply_t* reply =
      xcb_intern_atom_reply(ctx->connection, cookie, nullptr);

  if (!reply) {
    close(fd);
    return 0;
  }

  int flags, rv;

  xcb_convert_selection(ctx->connection, ctx->selection_window,
                        ctx->atoms[ATOM_CLIPBOARD].value, reply->atom,
                        ctx->atoms[ATOM_WL_SELECTION].value, XCB_CURRENT_TIME);

  flags = fcntl(fd, F_GETFL, 0);
  rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  errno_assert(!rv);

  ctx->selection_data_source_send_fd = fd;
  free(reply);
  return 1;
}

void sl_process_data_source_send_pending_list(struct sl_context* ctx) {
  while (!wl_list_empty(&ctx->selection_data_source_send_pending)) {
    struct wl_list* next = ctx->selection_data_source_send_pending.next;
    struct sl_data_source_send_request* request;
    request = wl_container_of(next, request, link);
    wl_list_remove(next);

    int rv = sl_begin_data_source_send(ctx, request->fd, request->cookie,
                                       request->data_source);
    free(request);
    if (rv)
      break;
  }
}

static int sl_handle_selection_fd_writable(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = static_cast<sl_context*>(data);
  int bytes, bytes_left;

  uint8_t* value = static_cast<uint8_t*>(
      xcb_get_property_value(ctx->selection_property_reply));
  bytes_left = xcb_get_property_value_length(ctx->selection_property_reply) -
               ctx->selection_property_offset;

  bytes = write(fd, value + ctx->selection_property_offset, bytes_left);
  if (bytes == -1) {
    LOG(ERROR) << "write error to target fd: " << strerror(errno);
    close(fd);
    fd = -1;
  } else if (bytes == bytes_left) {
    if (ctx->selection_incremental_transfer) {
      xcb_delete_property(ctx->connection, ctx->selection_window,
                          ctx->atoms[ATOM_WL_SELECTION].value);
    } else {
      close(fd);
      fd = -1;
    }
  } else {
    ctx->selection_property_offset += bytes;
    return 1;
  }

  free(ctx->selection_property_reply);
  ctx->selection_property_reply = nullptr;
  if (ctx->selection_send_event_source) {
    ctx->selection_send_event_source.reset();
  }
  if (fd < 0) {
    ctx->selection_data_source_send_fd = -1;
    sl_process_data_source_send_pending_list(ctx);
  }
  return 1;
}

static void sl_write_selection_property(struct sl_context* ctx,
                                        xcb_get_property_reply_t* reply) {
  ctx->selection_property_offset = 0;
  ctx->selection_property_reply = reply;
  sl_handle_selection_fd_writable(ctx->selection_data_source_send_fd,
                                  WL_EVENT_WRITABLE, ctx);

  if (!ctx->selection_property_reply)
    return;

  assert(!ctx->selection_send_event_source);
  ctx->selection_send_event_source.reset(wl_event_loop_add_fd(
      wl_display_get_event_loop(ctx->host_display),
      ctx->selection_data_source_send_fd, WL_EVENT_WRITABLE,
      sl_handle_selection_fd_writable, ctx));
}

static void sl_send_selection_notify(struct sl_context* ctx,
                                     xcb_atom_t property) {
  TRACE_EVENT("other", "sl_send_selection_notify");
  xcb_selection_notify_event_t event = {
      .response_type = XCB_SELECTION_NOTIFY,
      .pad0 = 0,
      .sequence = 0,
      .time = ctx->selection_request.time,
      .requestor = ctx->selection_request.requestor,
      .selection = ctx->selection_request.selection,
      .target = ctx->selection_request.target,
      .property = property};

  xcb_send_event(ctx->connection, 0, ctx->selection_request.requestor,
                 XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<char*>(&event));
}

static void sl_send_selection_data(struct sl_context* ctx) {
  assert(!ctx->selection_data_ack_pending);
  xcb_change_property(
      ctx->connection, XCB_PROP_MODE_REPLACE, ctx->selection_request.requestor,
      ctx->selection_request.property, ctx->selection_data_type,
      /*format=*/8, ctx->selection_data.size, ctx->selection_data.data);
  ctx->selection_data_ack_pending = 1;
  ctx->selection_data.size = 0;
}

static const uint32_t sl_incr_chunk_size = 64 * 1024;

static int sl_handle_selection_fd_readable(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = static_cast<sl_context*>(data);

  // When a selection starts, the wl_array in |ctx->selection_data| is
  // initialized with a size of zero. Since we now need to actually write into
  // it, allocate |sl_incr_chunk_size| bytes to store the selection data in. We
  // need to buffer this much to decide between a one-shot transfer and an
  // incremental transfer, as this decision must be made before the first
  // response is sent.
  if (ctx->selection_data.alloc == 0) {
    // wl_array_add is ostensibly failable, but the only failure case comes from
    // calling malloc, and if that fails we should just die anyway.
    errno_assert(
        (size_t)wl_array_add(&ctx->selection_data, sl_incr_chunk_size));

    // wl_array_add increments |size| as well as |alloc|, but we don't actually
    // want that yet. Instead we will set |size| later based on the results of
    // the read call.
    ctx->selection_data.size -= sl_incr_chunk_size;
  }

  int offset = ctx->selection_data.size;
  void* p = reinterpret_cast<char*>(ctx->selection_data.data) + offset;
  int bytes_left = ctx->selection_data.alloc - offset;

  int bytes = read(fd, p, bytes_left);
  if (bytes == -1) {
    LOG(ERROR) << "read error from data source: " << strerror(errno);
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
    ctx->selection_data_offer_receive_fd = -1;
    close(fd);
  } else {
    ctx->selection_data.size = offset + bytes;
    if (ctx->selection_data.size >= sl_incr_chunk_size) {
      if (!ctx->selection_incremental_transfer) {
        ctx->selection_incremental_transfer = 1;
        xcb_change_property(
            ctx->connection, XCB_PROP_MODE_REPLACE,
            ctx->selection_request.requestor, ctx->selection_request.property,
            ctx->atoms[ATOM_INCR].value, 32, 1, &sl_incr_chunk_size);
        ctx->selection_data_ack_pending = 1;
        sl_send_selection_notify(ctx, ctx->selection_request.property);
      } else if (!ctx->selection_data_ack_pending) {
        sl_send_selection_data(ctx);
      }
    } else if (bytes == 0) {
      if (!ctx->selection_data_ack_pending)
        sl_send_selection_data(ctx);
      if (!ctx->selection_incremental_transfer) {
        sl_send_selection_notify(ctx, ctx->selection_request.property);
        ctx->selection_request.requestor = XCB_NONE;
        wl_array_release(&ctx->selection_data);
      }
      xcb_flush(ctx->connection);
      ctx->selection_data_offer_receive_fd = -1;
      close(fd);
    } else {
      ctx->selection_data.size = offset + bytes;
      return 1;
    }
  }

  ctx->selection_event_source.reset();
  return 1;
}

void sl_handle_property_notify(struct sl_context* ctx,
                               xcb_property_notify_event_t* event) {
  TRACE_EVENT("x11wm", "XCB_PROPERTY_NOTIFY", [&](perfetto::EventContext p) {
    perfetto_annotate_atom(ctx, p, "event->atom", event->atom);
    perfetto_annotate_xcb_property_state(p, "event->state", event->state);
    perfetto_annotate_window(ctx, p, "event->window", event->window);
  });
  if (event->atom == XCB_ATOM_WM_NAME ||
      event->atom == ctx->atoms[ATOM_NET_WM_NAME].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    bool atom_is_net_wm_name =
        event->atom == ctx->atoms[ATOM_NET_WM_NAME].value;

    // If _NET_WM_NAME is set, ignore changes to WM_NAME.
    if (!atom_is_net_wm_name && window->has_net_wm_name)
      return;

    if (window->name) {
      free(window->name);
      window->name = nullptr;
      window->has_net_wm_name = false;
    }

    xcb_atom_t atom = XCB_ATOM_NONE;
    if (event->state != XCB_PROPERTY_DELETE)
      atom = event->atom;
    else if (atom_is_net_wm_name)
      // If _NET_WM_NAME is deleted, fall back to WM_NAME if it is set.
      atom = XCB_ATOM_WM_NAME;

    if (atom != XCB_ATOM_NONE) {
      xcb_get_property_reply_t* reply = xcb()->get_property_reply(
          ctx->connection,
          xcb()->get_property(ctx->connection, 0, window->id, atom,
                              XCB_ATOM_ANY, 0, 2048),
          nullptr);
      if (reply) {
        window->name =
            strndup(static_cast<char*>(xcb()->get_property_value(reply)),
                    xcb()->get_property_value_length(reply));
        free(reply);

        if (atom == ctx->atoms[ATOM_NET_WM_NAME].value) {
          window->has_net_wm_name = true;
        }
      }
    }

    if (!window->xdg_toplevel)
      return;

    if (window->name) {
      xdg_toplevel_set_title(window->xdg_toplevel, window->name);
    } else {
      xdg_toplevel_set_title(window->xdg_toplevel, "");
    }
  } else if (event->atom == XCB_ATOM_WM_CLASS) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window || event->state == XCB_PROPERTY_DELETE)
      return;

    xcb_get_property_cookie_t cookie =
        xcb()->get_property(ctx->connection, 0, window->id, XCB_ATOM_WM_CLASS,
                            XCB_ATOM_ANY, 0, 2048);
    xcb_get_property_reply_t* reply =
        xcb()->get_property_reply(ctx->connection, cookie, nullptr);
    if (reply) {
      sl_decode_wm_class(window, reply);
      free(reply);
    }
    sl_update_application_id(ctx, window);
  } else if (event->atom == ctx->atoms[ATOM_STEAM_GAME].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (window) {
      xcb_get_property_cookie_t cookie = xcb()->get_property(
          ctx->connection, 0, window->id, event->atom, XCB_ATOM_CARDINAL, 0, 1);
      xcb_get_property_reply_t* reply =
          xcb()->get_property_reply(ctx->connection, cookie, nullptr);
      on_steam_game_id_updated(ctx, window, reply);
    }
  } else if (event->atom == ctx->application_id_property_atom) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window || event->state == XCB_PROPERTY_DELETE)
      return;

    // TODO(cpelling): Support other atom types (e.g. strings) if/when a use
    // case arises. The current use case is for cardinals (uint32) but this
    // is easy enough to extend later.
    xcb_get_property_cookie_t cookie = xcb()->get_property(
        ctx->connection, 0, window->id, ctx->application_id_property_atom,
        XCB_ATOM_CARDINAL, 0, 1);
    xcb_get_property_reply_t* reply =
        xcb()->get_property_reply(ctx->connection, cookie, nullptr);
    if (reply) {
      sl_set_application_id_from_atom(ctx, window, reply);
      sl_update_application_id(ctx, window);
      free(reply);
    }
  } else if (event->atom == XCB_ATOM_WM_NORMAL_HINTS) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    // Don't report min/max size by default.
    window->size_flags &= ~(P_MIN_SIZE | P_MAX_SIZE);

    if (event->state != XCB_PROPERTY_DELETE) {
      struct sl_wm_size_hints size_hints = {0};
      xcb_get_property_reply_t* reply = xcb()->get_property_reply(
          ctx->connection,
          xcb()->get_property(ctx->connection, 0, window->id,
                              XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_ANY, 0,
                              sizeof(size_hints)),
          nullptr);
      if (reply) {
        memcpy(&size_hints, xcb()->get_property_value(reply),
               sizeof(size_hints));
        free(reply);
      }
      TRACE_EVENT("x11wm", "XCB_PROPERTY_NOTIFY: XCB_ATOM_WM_NORMAL_HINTS",
                  [&](perfetto::EventContext p) {
                    perfetto_annotate_size_hints(p, size_hints);
                  });

      window->size_flags |= size_hints.flags & (P_MIN_SIZE | P_MAX_SIZE);
      if (window->size_flags & P_MIN_SIZE) {
        window->min_width = size_hints.min_width;
        window->min_height = size_hints.min_height;
        LOG(VERBOSE) << window << " min=" << window->min_width << "x"
                     << window->min_height;
      }
      if (window->size_flags & P_MAX_SIZE) {
        if (size_hints.max_width < INT_MAX) {
          window->max_width = size_hints.max_width;
        } else {
          window->max_width = 0;
        }
        if (size_hints.max_height < INT_MAX) {
          window->max_height = size_hints.max_height;
        } else {
          window->max_height = 0;
        }
        LOG(VERBOSE) << window << " max=" << window->max_width << "x"
                     << window->max_height;
      }
    }

    if (!window->xdg_toplevel)
      return;

    if (window->size_flags & P_MIN_SIZE) {
      int32_t minw = window->min_width;
      int32_t minh = window->min_height;

      sl_transform_guest_to_host(window->ctx, window->paired_surface, &minw,
                                 &minh);
      xdg_toplevel_set_min_size(window->xdg_toplevel, minw, minh);
    } else {
      xdg_toplevel_set_min_size(window->xdg_toplevel, 0, 0);
    }

    if (window->size_flags & P_MAX_SIZE) {
      int32_t maxw = window->max_width;
      int32_t maxh = window->max_height;

      sl_transform_guest_to_host(window->ctx, window->paired_surface, &maxw,
                                 &maxh);
      xdg_toplevel_set_max_size(window->xdg_toplevel, maxw, maxh);
    } else {
      xdg_toplevel_set_max_size(window->xdg_toplevel, 0, 0);
    }
  } else if (event->atom == XCB_ATOM_WM_HINTS) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    if (event->state == XCB_PROPERTY_DELETE)
      return;
    struct sl_wm_hints wm_hints = {0};
    xcb_get_property_reply_t* reply = xcb()->get_property_reply(
        ctx->connection,
        xcb()->get_property(ctx->connection, 0, window->id, XCB_ATOM_WM_HINTS,
                            XCB_ATOM_ANY, 0, sizeof(wm_hints)),
        nullptr);

    if (!reply)
      return;
    memcpy(&wm_hints, xcb()->get_property_value(reply), sizeof(wm_hints));
    free(reply);

    if (wm_hints.flags & WM_HINTS_FLAG_URGENCY) {
      sl_request_attention(ctx, window, /*is_strong_request=*/false);
    }
  } else if (event->atom == ctx->atoms[ATOM_MOTIF_WM_HINTS].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    // Managed windows are decorated by default.
    window->decorated = window->managed;

    if (event->state != XCB_PROPERTY_DELETE) {
      struct sl_mwm_hints mwm_hints = {0};
      xcb_get_property_reply_t* reply = xcb()->get_property_reply(
          ctx->connection,
          xcb()->get_property(ctx->connection, 0, window->id,
                              ctx->atoms[ATOM_MOTIF_WM_HINTS].value,
                              XCB_ATOM_ANY, 0, sizeof(mwm_hints)),
          nullptr);
      if (reply) {
        if (xcb()->get_property_value_length(reply) >=
            static_cast<int>(sizeof(mwm_hints))) {
          memcpy(&mwm_hints, xcb()->get_property_value(reply),
                 sizeof(mwm_hints));
        }
        free(reply);
        if (mwm_hints.flags & MWM_HINTS_DECORATIONS) {
          if (mwm_hints.decorations & MWM_DECOR_ALL)
            window->decorated = ~mwm_hints.decorations & MWM_DECOR_TITLE;
          else
            window->decorated = mwm_hints.decorations & MWM_DECOR_TITLE;
        }
      }
    }

    if (!window->aura_surface)
      return;

    zaura_surface_set_frame(window->aura_surface,
                            window->decorated ? ZAURA_SURFACE_FRAME_TYPE_NORMAL
                            : window->depth == 32
                                ? ZAURA_SURFACE_FRAME_TYPE_NONE
                                : ZAURA_SURFACE_FRAME_TYPE_SHADOW);
  } else if (event->atom == ctx->atoms[ATOM_GTK_THEME_VARIANT].value) {
    struct sl_window* window;
    uint32_t frame_color;

    window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    window->dark_frame = 0;

    if (event->state != XCB_PROPERTY_DELETE) {
      xcb_get_property_reply_t* reply = xcb()->get_property_reply(
          ctx->connection,
          xcb()->get_property(ctx->connection, 0, window->id,
                              ctx->atoms[ATOM_GTK_THEME_VARIANT].value,
                              XCB_ATOM_ANY, 0, 2048),
          nullptr);
      if (reply) {
        if (xcb()->get_property_value_length(reply) >= 4)
          window->dark_frame = !strcmp(
              static_cast<char*>(xcb()->get_property_value(reply)), "dark");
        free(reply);
      }
    }

    if (!window->aura_surface)
      return;

    frame_color = window->dark_frame ? ctx->dark_frame_color : ctx->frame_color;
    zaura_surface_set_frame_colors(window->aura_surface, frame_color,
                                   frame_color);
  } else if (event->atom ==
             ctx->atoms[ATOM_XWAYLAND_RANDR_EMU_MONITOR_RECTS].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);

    if (!window) {
      TRACE_EVENT("x11wm",
                  "XCB_PROPERTY_NOTIFY: _XWAYLAND_RANDR_EMU_MONITOR_RECTS",
                  [&](perfetto::EventContext p) {
                    perfetto_annotate_window(ctx, p, "window", event->window);
                  });
      return;
    }

    xcb_get_property_reply_t* reply = xcb()->get_property_reply(
        ctx->connection,
        xcb()->get_property(
            ctx->connection, 0, window->id,
            ctx->atoms[ATOM_XWAYLAND_RANDR_EMU_MONITOR_RECTS].value,
            XCB_ATOM_CARDINAL, 0, 2048),
        nullptr);

    xcb_get_property_reply_t* reply_for_trace =
        static_cast<xcb_get_property_reply_t*>(
            malloc(sizeof(xcb_get_property_reply_t)));
    memcpy(reply_for_trace, reply, sizeof(*reply));
    TRACE_EVENT("x11wm",
                "XCB_PROPERTY_NOTIFY: _XWAYLAND_RANDR_EMU_MONITOR_RECTS",
                [&](perfetto::EventContext p) {
                  perfetto_annotate_window(ctx, p, "window", event->window);
                  perfetto_annotate_cardinal_list(p, "value", reply_for_trace);
                  free(reply_for_trace);
                });

    if (!ctx->allow_xwayland_emulate_screen_pos_size) {
      return;
    }

    if (!reply || reply->value_len == 0 || reply->value_len % 4 != 0) {
      // Stop using emulated rects when:
      // - the window property is unset (ie. len == 0), we are back to native
      // - the window property length is not a multiple of 4 (property invalid)
      //   resolution
      // - we failed to get reply
      LOG(VERBOSE) << window << " _RANDR_EMU_MONITOR_RECTS unset";
      window->use_emulated_rects = false;
      free(reply);
      return;
    }

    uint32_t* xywh = static_cast<uint32_t*>(xcb()->get_property_value(reply));
    if (!xywh) {
      // The values are probably incorrectly set by XWayland, consider it as
      // buggy and do not proceed further.
      LOG(VERBOSE) << window << " _RANDR_EMU_MONITOR_RECTS invalid values";
      window->use_emulated_rects = false;
      free(reply);
      return;
    }

    // We need to select the appropriate emulated screen based on the screen the
    // window is in.
    uint32_t target_x = 0;
    uint32_t target_y = 0;
    if (!sl_window_get_output_virt_position(window, target_x, target_y)) {
      LOG(ERROR) << window
                 << " _RANDR_EMU_MONITOR_RECTS failed to find output it is in";
      window->use_emulated_rects = false;
      free(reply);
      return;
    }

    for (uint32_t i = 0; i < reply->value_len; i += 4) {
      // Assume the fullscreen window position is the screen's position.
      // XWayland emulation must match this x/y coordinates.
      if (xywh[i] == target_x && xywh[i + 1] == target_y) {
        window->emulated_x = xywh[i];
        window->emulated_y = xywh[i + 1];
        window->emulated_width = xywh[i + 2];
        window->emulated_height = xywh[i + 3];
        window->use_emulated_rects = true;
        // XWayland will send a wp_viewport.set_source and destination request
        // (plus, do other busy work such as setting pointer scaling) based on
        // window property set. Configure window manually here to trigger
        // necessary changes since some games do not do anything further than
        // adding the attributes.
        xcb()->configure_window(window->ctx->connection, window->id,
                                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                                    XCB_CONFIG_WINDOW_WIDTH |
                                    XCB_CONFIG_WINDOW_HEIGHT,
                                &xywh[i]);
        LOG(VERBOSE) << window << " _RANDR_EMU_MONITOR_RECTS set as "
                     << xywh[i + 2] << "x" << xywh[i + 3] << " (" << xywh[i]
                     << "," << xywh[i + 1] << ")";
        free(reply);
        return;
      }
    }

    LOG(ERROR)
        << "_RANDR_EMU_MONITOR_RECTS failed to find screen with position "
        << target_x << ", " << target_y;
    window->use_emulated_rects = false;
    free(reply);

  } else if (event->atom == ctx->atoms[ATOM_WL_SELECTION].value) {
    if (event->window == ctx->selection_window &&
        event->state == XCB_PROPERTY_NEW_VALUE &&
        ctx->selection_incremental_transfer) {
      xcb_get_property_reply_t* reply = xcb()->get_property_reply(
          ctx->connection,
          xcb()->get_property(ctx->connection, 0, ctx->selection_window,
                              ctx->atoms[ATOM_WL_SELECTION].value,
                              XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff),
          nullptr);

      if (!reply)
        return;

      if (xcb()->get_property_value_length(reply) > 0) {
        sl_write_selection_property(ctx, reply);
      } else {
        assert(!ctx->selection_send_event_source);
        close(ctx->selection_data_source_send_fd);
        ctx->selection_data_source_send_fd = -1;
        free(reply);

        sl_process_data_source_send_pending_list(ctx);
      }
    }
  } else if (event->atom == ctx->selection_request.property) {
    if (event->window == ctx->selection_request.requestor &&
        event->state == XCB_PROPERTY_DELETE &&
        ctx->selection_incremental_transfer) {
      int data_size = ctx->selection_data.size;

      ctx->selection_data_ack_pending = 0;

      // Handle the case when there's more data to be received.
      if (ctx->selection_data_offer_receive_fd >= 0) {
        // Avoid sending empty data until transfer is complete.
        if (data_size)
          sl_send_selection_data(ctx);

        if (!ctx->selection_event_source) {
          ctx->selection_event_source.reset(wl_event_loop_add_fd(
              wl_display_get_event_loop(ctx->host_display),
              ctx->selection_data_offer_receive_fd, WL_EVENT_READABLE,
              sl_handle_selection_fd_readable, ctx));
        }
        return;
      }

      sl_send_selection_data(ctx);

      // Release data if transfer is complete.
      if (!data_size) {
        ctx->selection_request.requestor = XCB_NONE;
        wl_array_release(&ctx->selection_data);
      }
    }
  }
}

static void sl_internal_data_source_send(void* data,
                                         struct wl_data_source* data_source,
                                         const char* mime_type,
                                         int32_t fd) {
  TRACE_EVENT("other", "sl_internal_data_source_send");
  struct sl_data_source* host = static_cast<sl_data_source*>(data);
  struct sl_context* ctx = host->ctx;

  xcb_intern_atom_cookie_t cookie =
      xcb_intern_atom(ctx->connection, false, strlen(mime_type), mime_type);

  if (ctx->selection_data_source_send_fd < 0) {
    sl_begin_data_source_send(ctx, fd, cookie, host);
  } else {
    struct sl_data_source_send_request* request =
        static_cast<sl_data_source_send_request*>(
            malloc(sizeof(struct sl_data_source_send_request)));

    request->fd = fd;
    request->cookie = cookie;
    request->data_source = host;
    wl_list_insert(&ctx->selection_data_source_send_pending, &request->link);
  }
}

static void sl_internal_data_source_cancelled(
    void* data, struct wl_data_source* data_source) {
  TRACE_EVENT("other", "sl_internal_data_source_cancelled");
  struct sl_data_source* host = static_cast<sl_data_source*>(data);

  if (host->ctx->selection_data_source == host)
    host->ctx->selection_data_source = nullptr;

  wl_data_source_destroy(data_source);
}

static const struct wl_data_source_listener sl_internal_data_source_listener = {
    /*target=*/DoNothing, sl_internal_data_source_send,
    sl_internal_data_source_cancelled};

char* sl_copy_atom_name(xcb_get_atom_name_reply_t* reply) {
  // The string produced by xcb_get_atom_name_name isn't null terminated, so we
  // have to copy |name_len| bytes into a new buffer and add the null character
  // ourselves.
  char* name_start = xcb_get_atom_name_name(reply);
  int name_len = xcb_get_atom_name_name_length(reply);
  char* name = static_cast<char*>(malloc(name_len + 1));
  memcpy(name, name_start, name_len);
  name[name_len] = '\0';
  return name;
}

static void sl_get_selection_targets(struct sl_context* ctx) {
  TRACE_EVENT("other", "sl_get_selection_targets");
  struct sl_data_source* data_source = nullptr;
  xcb_get_property_reply_t* reply;
  xcb_atom_t* value;
  uint32_t i;

  reply = xcb_get_property_reply(
      ctx->connection,
      xcb_get_property(ctx->connection, 1, ctx->selection_window,
                       ctx->atoms[ATOM_WL_SELECTION].value,
                       XCB_GET_PROPERTY_TYPE_ANY, 0, DEFAULT_BUFFER_SIZE),
      nullptr);
  if (!reply)
    return;

  if (reply->type != XCB_ATOM_ATOM) {
    free(reply);
    return;
  }

  if (ctx->data_device_manager) {
    data_source = static_cast<sl_data_source*>(malloc(sizeof(*data_source)));
    assert(data_source);

    data_source->ctx = ctx;
    data_source->internal = wl_data_device_manager_create_data_source(
        ctx->data_device_manager->internal);
    wl_data_source_add_listener(data_source->internal,
                                &sl_internal_data_source_listener, data_source);

    value = static_cast<xcb_atom_t*>(xcb_get_property_value(reply));

    // We need to convert all of the offered target types from X11 atoms to
    // strings (i.e. getting the names of the atoms). Each conversion requires a
    // round trip to the X server, but none of the requests depend on each
    // other. Therefore, we can speed things up by sending out all the requests
    // as a batch with xcb_get_atom_name, and then read all the replies as a
    // batch with xcb_get_atom_name_reply.
    xcb_get_atom_name_cookie_t* atom_name_cookies =
        static_cast<xcb_get_atom_name_cookie_t*>(
            malloc(sizeof(xcb_get_atom_name_cookie_t) * reply->value_len));
    for (i = 0; i < reply->value_len; i++) {
      atom_name_cookies[i] = xcb_get_atom_name(ctx->connection, value[i]);
    }
    for (i = 0; i < reply->value_len; i++) {
      xcb_get_atom_name_reply_t* atom_name_reply = xcb_get_atom_name_reply(
          ctx->connection, atom_name_cookies[i], nullptr);
      if (atom_name_reply) {
        char* name = sl_copy_atom_name(atom_name_reply);
        wl_data_source_offer(data_source->internal, name);
        free(atom_name_reply);
        free(name);
      }
    }
    free(atom_name_cookies);

    if (ctx->selection_data_device && ctx->default_seat) {
      wl_data_device_set_selection(ctx->selection_data_device,
                                   data_source->internal,
                                   ctx->default_seat->seat->last_serial);
    }

    if (ctx->selection_data_source) {
      wl_data_source_destroy(ctx->selection_data_source->internal);
      free(ctx->selection_data_source);
    }
    ctx->selection_data_source = data_source;
  }

  free(reply);
}

static void sl_get_selection_data(struct sl_context* ctx) {
  TRACE_EVENT("other", "sl_get_selection_data");
  xcb_get_property_reply_t* reply = xcb_get_property_reply(
      ctx->connection,
      xcb_get_property(ctx->connection, 1, ctx->selection_window,
                       ctx->atoms[ATOM_WL_SELECTION].value,
                       XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff),
      nullptr);
  if (!reply)
    return;

  if (reply->type == ctx->atoms[ATOM_INCR].value) {
    ctx->selection_incremental_transfer = 1;
    free(reply);
  } else {
    ctx->selection_incremental_transfer = 0;
    sl_write_selection_property(ctx, reply);
  }
}

static void sl_handle_selection_notify(struct sl_context* ctx,
                                       xcb_selection_notify_event_t* event) {
  if (event->property == XCB_ATOM_NONE)
    return;

  if (event->target == ctx->atoms[ATOM_TARGETS].value)
    sl_get_selection_targets(ctx);
  else
    sl_get_selection_data(ctx);
}

static void sl_send_targets(struct sl_context* ctx) {
  xcb_change_property(
      ctx->connection, XCB_PROP_MODE_REPLACE, ctx->selection_request.requestor,
      ctx->selection_request.property, XCB_ATOM_ATOM, 32,
      ctx->selection_data_offer->atoms.size / sizeof(xcb_atom_t),
      ctx->selection_data_offer->atoms.data);

  sl_send_selection_notify(ctx, ctx->selection_request.property);
}

static void sl_send_timestamp(struct sl_context* ctx) {
  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE,
                      ctx->selection_request.requestor,
                      ctx->selection_request.property, XCB_ATOM_INTEGER, 32, 1,
                      &ctx->selection_timestamp);

  sl_send_selection_notify(ctx, ctx->selection_request.property);
}

static void sl_send_data(struct sl_context* ctx, xcb_atom_t data_type) {
  TRACE_EVENT("other", "sl_send_data");
  int rv, fd_to_receive, fd_to_wayland;

  if (!ctx->selection_data_offer) {
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
    return;
  }

  if (ctx->selection_event_source) {
    LOG(ERROR) << "error: selection transfer already pending";
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
    return;
  }

  ctx->selection_data_type = data_type;

  // We will need the name of this atom later to tell the wayland server what
  // type of data to send us, so start the request now.
  xcb_get_atom_name_cookie_t atom_name_cookie =
      xcb_get_atom_name(ctx->connection, data_type);

  wl_array_init(&ctx->selection_data);
  ctx->selection_data_ack_pending = 0;

  if (ctx->channel == nullptr) {
    // Running in noop mode, without virtualization.
    int p[2];

    rv = pipe2(p, O_CLOEXEC | O_NONBLOCK);
    errno_assert(!rv);

    fd_to_receive = p[0];
    fd_to_wayland = p[1];
  } else {
    int pipe_fd;
    rv = ctx->channel->create_pipe(pipe_fd);
    if (rv) {
      LOG(ERROR) << "error: failed to create virtwl pipe: " << strerror(-rv);
      sl_send_selection_notify(ctx, XCB_ATOM_NONE);
      return;
    }

    fd_to_receive = pipe_fd;
    fd_to_wayland = pipe_fd;
  }

  xcb_get_atom_name_reply_t* atom_name_reply =
      xcb_get_atom_name_reply(ctx->connection, atom_name_cookie, nullptr);
  if (atom_name_reply) {
    // If we got the atom name, then send the request to wayland and add our end
    // of the pipe to the wayland event loop.
    ctx->selection_data_offer_receive_fd = fd_to_receive;
    char* name = sl_copy_atom_name(atom_name_reply);
    wl_data_offer_receive(ctx->selection_data_offer->internal, name,
                          fd_to_wayland);
    free(atom_name_reply);
    free(name);

    ctx->selection_event_source.reset(wl_event_loop_add_fd(
        wl_display_get_event_loop(ctx->host_display),
        ctx->selection_data_offer_receive_fd, WL_EVENT_READABLE,
        sl_handle_selection_fd_readable, ctx));
  } else {
    // If getting the atom name failed, notify the requestor that there won't be
    // any data, and close our end of the pipe.
    close(fd_to_receive);
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
  }

  // Close the wayland end of the pipe, now that it's either been sent or not
  // going to be sent. The VIRTWL driver uses the same fd for both ends of the
  // pipe, so don't close the fd if both ends are the same.
  if (fd_to_receive != fd_to_wayland)
    close(fd_to_wayland);
}

static void sl_handle_selection_request(struct sl_context* ctx,
                                        xcb_selection_request_event_t* event) {
  ctx->selection_request = *event;
  ctx->selection_incremental_transfer = 0;

  if (event->selection == ctx->atoms[ATOM_CLIPBOARD_MANAGER].value) {
    sl_send_selection_notify(ctx, ctx->selection_request.property);
    return;
  }

  if (event->target == ctx->atoms[ATOM_TARGETS].value) {
    sl_send_targets(ctx);
  } else if (event->target == ctx->atoms[ATOM_TIMESTAMP].value) {
    sl_send_timestamp(ctx);
  } else {
    int success = 0;
    xcb_atom_t* atom;
    sl_array_for_each(atom, &ctx->selection_data_offer->atoms) {
      if (event->target == *atom) {
        success = 1;
        sl_send_data(ctx, *atom);
        break;
      }
    }
    if (!success) {
      sl_send_selection_notify(ctx, XCB_ATOM_NONE);
    }
  }
}

static void sl_handle_xfixes_selection_notify(
    struct sl_context* ctx, xcb_xfixes_selection_notify_event_t* event) {
  if (event->selection != ctx->atoms[ATOM_CLIPBOARD].value)
    return;

  if (event->owner == XCB_WINDOW_NONE) {
    // If client selection is gone. Set nullptr selection for each seat.
    if (ctx->selection_owner != ctx->selection_window) {
      if (ctx->selection_data_device && ctx->default_seat) {
        wl_data_device_set_selection(ctx->selection_data_device, nullptr,
                                     ctx->default_seat->seat->last_serial);
      }
    }
    ctx->selection_owner = XCB_WINDOW_NONE;
    return;
  }

  ctx->selection_owner = event->owner;

  // Save timestamp if it's our selection.
  if (event->owner == ctx->selection_window) {
    ctx->selection_timestamp = event->timestamp;
    return;
  }

  ctx->selection_incremental_transfer = 0;
  xcb_convert_selection(ctx->connection, ctx->selection_window,
                        ctx->atoms[ATOM_CLIPBOARD].value,
                        ctx->atoms[ATOM_TARGETS].value,
                        ctx->atoms[ATOM_WL_SELECTION].value, event->timestamp);
}

static int sl_handle_x_connection_event(int fd, uint32_t mask, void* data) {
  TRACE_EVENT("other", "sl_handle_x_connection_event");
  struct sl_context* ctx = (struct sl_context*)data;
  xcb_generic_event_t* event;
  uint32_t count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
    LOG(FATAL) << "got error or hangup (mask " << mask
               << ") on X connection, exiting";
    exit(EXIT_SUCCESS);
  }

  while ((event = xcb_poll_for_event(ctx->connection))) {
    switch (event->response_type & ~SEND_EVENT_MASK) {
      case XCB_CREATE_NOTIFY:
        sl_handle_create_notify(
            ctx, reinterpret_cast<xcb_create_notify_event_t*>(event));
        break;
      case XCB_DESTROY_NOTIFY:
        sl_handle_destroy_notify(
            ctx, reinterpret_cast<xcb_destroy_notify_event_t*>(event));
        break;
      case XCB_REPARENT_NOTIFY:
        sl_handle_reparent_notify(
            ctx, reinterpret_cast<xcb_reparent_notify_event_t*>(event));
        break;
      case XCB_MAP_REQUEST:
        sl_handle_map_request(
            ctx, reinterpret_cast<xcb_map_request_event_t*>(event));
        break;
      case XCB_MAP_NOTIFY:
        sl_handle_map_notify(ctx,
                             reinterpret_cast<xcb_map_notify_event_t*>(event));
        break;
      case XCB_UNMAP_NOTIFY:
        sl_handle_unmap_notify(
            ctx, reinterpret_cast<xcb_unmap_notify_event_t*>(event));
        break;
      case XCB_CONFIGURE_REQUEST:
        sl_handle_configure_request(
            ctx, reinterpret_cast<xcb_configure_request_event_t*>(event));
        break;
      case XCB_CONFIGURE_NOTIFY:
        sl_handle_configure_notify(
            ctx, reinterpret_cast<xcb_configure_notify_event_t*>(event));
        break;
      case XCB_CLIENT_MESSAGE:
        sl_handle_client_message(
            ctx, reinterpret_cast<xcb_client_message_event_t*>(event));
        break;
      case XCB_FOCUS_IN:
        sl_handle_focus_in(ctx, reinterpret_cast<xcb_focus_in_event_t*>(event));
        break;
      case XCB_FOCUS_OUT:
        sl_handle_focus_out(ctx,
                            reinterpret_cast<xcb_focus_out_event_t*>(event));
        break;
      case XCB_PROPERTY_NOTIFY:
        sl_handle_property_notify(
            ctx, reinterpret_cast<xcb_property_notify_event_t*>(event));
        break;
      case XCB_SELECTION_NOTIFY:
        sl_handle_selection_notify(
            ctx, reinterpret_cast<xcb_selection_notify_event_t*>(event));
        break;
      case XCB_SELECTION_REQUEST:
        sl_handle_selection_request(
            ctx, reinterpret_cast<xcb_selection_request_event_t*>(event));
        break;
    }

    switch (event->response_type - ctx->xfixes_extension->first_event) {
      case XCB_XFIXES_SELECTION_NOTIFY:
        sl_handle_xfixes_selection_notify(
            ctx, reinterpret_cast<xcb_xfixes_selection_notify_event_t*>(event));
        break;
    }

    // Xshape specific events extend the normal event numbers
    // The first event id is retrieved when querying for xshape
    // extension information. This can be used to determine
    // if the event that is received is Xshape specific.
    if (ctx->enable_xshape) {
      uint8_t xshape_event_id =
          event->response_type - ctx->xshape_extension->first_event;
      switch (xshape_event_id) {
        case XCB_SHAPE_NOTIFY:
          sl_handle_shape_notify(
              ctx, reinterpret_cast<xcb_shape_notify_event_t*>(event));
          break;
      }
    }

    free(event);
    ++count;
  }

  if ((mask & ~WL_EVENT_WRITABLE) == 0)
    xcb_flush(ctx->connection);

  return count;
}

static void sl_set_supported(struct sl_context* ctx) {
  const xcb_atom_t supported_atoms[] = {
      ctx->atoms[ATOM_NET_ACTIVE_WINDOW].value,
      ctx->atoms[ATOM_NET_WM_MOVERESIZE].value,
      ctx->atoms[ATOM_NET_WM_NAME].value,
      ctx->atoms[ATOM_NET_WM_STATE].value,
      ctx->atoms[ATOM_NET_WM_STATE_FULLSCREEN].value,
      ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT].value,
      ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ].value,
      ctx->atoms[ATOM_NET_WM_STATE_FOCUSED].value,
      // TODO(hollingum): STATE_MODAL and CLIENT_LIST, based on what wlroots
      // has.
  };

  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->screen->root,
                      ctx->atoms[ATOM_NET_SUPPORTED].value, XCB_ATOM_ATOM, 32,
                      sizeof(supported_atoms) / sizeof(xcb_atom_t),
                      supported_atoms);
}

// The window manager is responsible for setting the default cursor on the root
// window, which will be used by applications that do not try to set their own
// cursor. If we do not do this, the cursor will be invisible, see b/204724669
// for details.
static void sl_initialize_cursor(struct sl_context* ctx) {
  [[maybe_unused]] xcb_generic_error_t* err;
  xcb_void_cookie_t cookie;

  // Load the "cursor" font, which is a built-in set of fonts, documented at
  // https://tronche.com/gui/x/xlib/appendix/b/.
  xcb_font_t font_id = xcb_generate_id(ctx->connection);
  cookie = xcb_open_font_checked(ctx->connection, font_id, strlen("cursor"),
                                 "cursor");
  err = xcb_request_check(ctx->connection, cookie);
  assert(!err);

  // Create the standard "left ptr" cursor. The below call is a bit technical,
  // but it means:
  //  - font_id appears twice because we use a mask (background) whose font is
  //    the same as the source.
  //  - the mask is the source + 1, because the inbuilt cursor font has
  //    even-numbered foreground glyphs and odd-numbered backgrounds.
  //  - The numbers are 16-bit RGB for the foreground and background, so this
  //    has a black foreground and a white background.
  constexpr int left_ptr_cursor_id = 68;
  xcb_cursor_t cursor_id = xcb_generate_id(ctx->connection);
  cookie = xcb_create_glyph_cursor_checked(
      ctx->connection, cursor_id, font_id, font_id, left_ptr_cursor_id,
      left_ptr_cursor_id + 1, 0, 0, 0, 65535, 65535, 65535);
  err = xcb_request_check(ctx->connection, cookie);
  assert(!err);

  // Set the cursor for the root window.
  cookie = xcb_change_window_attributes_checked(
      ctx->connection, ctx->screen->root, XCB_CW_CURSOR, &cursor_id);
  err = xcb_request_check(ctx->connection, cookie);
  assert(!err);

  // Free the cursor and the fonts, since we no-longer need them.
  xcb_free_cursor(ctx->connection, cursor_id);
  cookie = xcb_close_font_checked(ctx->connection, font_id);
  err = xcb_request_check(ctx->connection, cookie);
  assert(!err);
}

static void sl_connect(struct sl_context* ctx) {
  TRACE_EVENT("other", "sl_connect");
  const char wm_name[] = "Sommelier";
  const xcb_setup_t* setup;
  xcb_screen_iterator_t screen_iterator;
  uint32_t values[1];
  xcb_void_cookie_t change_attributes_cookie, redirect_subwindows_cookie;
  xcb_generic_error_t* error;
  xcb_intern_atom_reply_t* atom_reply;
  xcb_depth_iterator_t depth_iterator;
  xcb_xfixes_query_version_reply_t* xfixes_query_version_reply;
  xcb_shape_query_version_reply_t* xshape_query_version_reply;
  const xcb_query_extension_reply_t* composite_extension;
  unsigned i;

  set_xcb_shim(new XcbShim());

  ctx->connection = xcb_connect_to_fd(ctx->wm_fd, nullptr);
  assert(!xcb_connection_has_error(ctx->connection));

  xcb_prefetch_extension_data(ctx->connection, &xcb_xfixes_id);
  xcb_prefetch_extension_data(ctx->connection, &xcb_composite_id);

  // Send requests to fetch/create ("intern") all the atoms we'll need later.
  for (i = 0; i < ARRAY_SIZE(ctx->atoms); ++i) {
    const char* name = ctx->atoms[i].name;
    ctx->atoms[i].cookie =
        xcb_intern_atom(ctx->connection, 0, strlen(name), name);
  }
  xcb_intern_atom_cookie_t app_id_atom_cookie;
  if (ctx->application_id_property_name) {
    app_id_atom_cookie = xcb_intern_atom(
        ctx->connection, 0, strlen(ctx->application_id_property_name),
        ctx->application_id_property_name);
  }

  setup = xcb_get_setup(ctx->connection);
  screen_iterator = xcb_setup_roots_iterator(setup);
  ctx->screen = screen_iterator.data;

  // Select for substructure redirect.
  values[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY |
              XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
              XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
  change_attributes_cookie = xcb_change_window_attributes(
      ctx->connection, ctx->screen->root, XCB_CW_EVENT_MASK, values);

  ctx->connection_event_source.reset(wl_event_loop_add_fd(
      wl_display_get_event_loop(ctx->host_display),
      xcb_get_file_descriptor(ctx->connection), WL_EVENT_READABLE,
      &sl_handle_x_connection_event, ctx));

  ctx->xfixes_extension =
      xcb_get_extension_data(ctx->connection, &xcb_xfixes_id);
  assert(ctx->xfixes_extension->present);

  xfixes_query_version_reply = xcb_xfixes_query_version_reply(
      ctx->connection,
      xcb_xfixes_query_version(ctx->connection, XCB_XFIXES_MAJOR_VERSION,
                               XCB_XFIXES_MINOR_VERSION),
      nullptr);
  assert(xfixes_query_version_reply);
  assert(xfixes_query_version_reply->major_version >= 5);
  free(xfixes_query_version_reply);

  composite_extension =
      xcb_get_extension_data(ctx->connection, &xcb_composite_id);
  assert(composite_extension->present);
  UNUSED(composite_extension);

  if (ctx->enable_xshape) {
    xcb_prefetch_extension_data(ctx->connection, &xcb_shape_id);

    ctx->xshape_extension =
        xcb_get_extension_data(ctx->connection, &xcb_shape_id);

    xshape_query_version_reply = xcb_shape_query_version_reply(
        ctx->connection, xcb_shape_query_version(ctx->connection), nullptr);

    assert(xshape_query_version_reply);
    assert(xshape_query_version_reply->major_version >=
           XCB_SHAPE_MAJOR_VERSION);
    assert(xshape_query_version_reply->minor_version >=
           XCB_SHAPE_MINOR_VERSION);
    free(xshape_query_version_reply);
    xshape_query_version_reply = nullptr;
  }

  redirect_subwindows_cookie = xcb_composite_redirect_subwindows_checked(
      ctx->connection, ctx->screen->root, XCB_COMPOSITE_REDIRECT_MANUAL);

  // Another window manager should not be running.
  error = xcb_request_check(ctx->connection, change_attributes_cookie);
  assert(!error);

  // Redirecting subwindows of root for compositing should have succeeded.
  error = xcb_request_check(ctx->connection, redirect_subwindows_cookie);
  assert(!error);

  ctx->window = xcb_generate_id(ctx->connection);
  xcb_create_window(ctx->connection, 0, ctx->window, ctx->screen->root, 0, 0, 1,
                    1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0,
                    nullptr);

  // Wait on results for all the atom intern requests we sent above.
  for (i = 0; i < ARRAY_SIZE(ctx->atoms); ++i) {
    atom_reply =
        xcb_intern_atom_reply(ctx->connection, ctx->atoms[i].cookie, &error);
    assert(!error);
    ctx->atoms[i].value = atom_reply->atom;
    free(atom_reply);
  }
  if (ctx->application_id_property_name) {
    atom_reply =
        xcb_intern_atom_reply(ctx->connection, app_id_atom_cookie, &error);
    assert(!error);
    ctx->application_id_property_atom = atom_reply->atom;
    free(atom_reply);
  }

  depth_iterator = xcb_screen_allowed_depths_iterator(ctx->screen);
  while (depth_iterator.rem > 0) {
    int depth = depth_iterator.data->depth;
    if (depth == ctx->screen->root_depth) {
      ctx->visual_ids[depth] = ctx->screen->root_visual;
      ctx->colormaps[depth] = ctx->screen->default_colormap;
    } else {
      xcb_visualtype_iterator_t visualtype_iterator =
          xcb_depth_visuals_iterator(depth_iterator.data);

      ctx->visual_ids[depth] = visualtype_iterator.data->visual_id;
      ctx->colormaps[depth] = xcb_generate_id(ctx->connection);
      xcb_create_colormap(ctx->connection, XCB_COLORMAP_ALLOC_NONE,
                          ctx->colormaps[depth], ctx->screen->root,
                          ctx->visual_ids[depth]);
    }
    xcb_depth_next(&depth_iterator);
  }
  assert(ctx->visual_ids[ctx->screen->root_depth]);

  if (ctx->clipboard_manager) {
    values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
    ctx->selection_window = xcb_generate_id(ctx->connection);
    xcb_create_window(ctx->connection, XCB_COPY_FROM_PARENT,
                      ctx->selection_window, ctx->screen->root, 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, ctx->screen->root_visual,
                      XCB_CW_EVENT_MASK, values);
    xcb_set_selection_owner(ctx->connection, ctx->selection_window,
                            ctx->atoms[ATOM_CLIPBOARD_MANAGER].value,
                            XCB_CURRENT_TIME);
    xcb_xfixes_select_selection_input(
        ctx->connection, ctx->selection_window,
        ctx->atoms[ATOM_CLIPBOARD].value,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE);
    sl_set_selection(ctx, nullptr);
  }

  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->window,
                      ctx->atoms[ATOM_NET_SUPPORTING_WM_CHECK].value,
                      XCB_ATOM_WINDOW, 32, 1, &ctx->window);
  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->window,
                      ctx->atoms[ATOM_NET_WM_NAME].value,
                      ctx->atoms[ATOM_UTF8_STRING].value, 8, strlen(wm_name),
                      wm_name);
  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->screen->root,
                      ctx->atoms[ATOM_NET_SUPPORTING_WM_CHECK].value,
                      XCB_ATOM_WINDOW, 32, 1, &ctx->window);
  sl_set_supported(ctx);
  xcb_set_selection_owner(ctx->connection, ctx->window,
                          ctx->atoms[ATOM_WM_S0].value, XCB_CURRENT_TIME);

  xcb_set_input_focus(ctx->connection, XCB_INPUT_FOCUS_NONE, XCB_NONE,
                      XCB_CURRENT_TIME);
  xcb_flush(ctx->connection);

  sl_initialize_cursor(ctx);
}

static void sl_sd_notify(const char* state) {
  const char* socket_name;
  struct msghdr msghdr;
  struct iovec iovec;
  struct sockaddr_un addr;
  int fd;
  int rv;

  socket_name = getenv("NOTIFY_SOCKET");
  assert(socket_name);

  fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  errno_assert(fd >= 0);

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_name, sizeof(addr.sun_path));
  // ensure path is null-terminated
  addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

  memset(&iovec, 0, sizeof(iovec));
  // iovec is just going to be used to send data as part of a const msghdr.
  iovec.iov_base = const_cast<char*>(state);
  iovec.iov_len = strlen(state);

  memset(&msghdr, 0, sizeof(msghdr));
  msghdr.msg_name = &addr;
  msghdr.msg_namelen =
      offsetof(struct sockaddr_un, sun_path) + strlen(socket_name);
  msghdr.msg_iov = &iovec;
  msghdr.msg_iovlen = 1;

  rv = sendmsg(fd, &msghdr, MSG_NOSIGNAL);
  errno_assert(rv != -1);
}

static int sl_handle_sigchld(int signal_number, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (pid == ctx->child_pid) {
      ctx->child_pid = -1;
      if (WIFEXITED(status) && WEXITSTATUS(status)) {
        LOG(ERROR) << "child exited with status: " << WEXITSTATUS(status);
      }
      if (ctx->exit_with_child) {
        if (ctx->xwayland_pid >= 0)
          kill(ctx->xwayland_pid, SIGTERM);
      } else {
        // Notify systemd that we are ready to accept connections now that
        // child process has finished running and all environment is ready.
        if (ctx->sd_notify)
          sl_sd_notify(ctx->sd_notify);
      }
    } else if (pid == ctx->xwayland_pid) {
      ctx->xwayland_pid = -1;
      if (WIFEXITED(status) && WEXITSTATUS(status)) {
        LOG(FATAL) << "Xwayland exited with status: " << WEXITSTATUS(status);
        exit(WEXITSTATUS(status));
      }
    }
  }

  return 1;
}

static int sl_handle_sigusr1(int signal_number, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  LOG(INFO) << "dumping trace " << ctx->trace_filename;
  dump_trace(ctx->trace_filename);
  if (ctx->timing != nullptr) {
    ctx->timing->OutputLog();
  }
  return 1;
}

static int sl_handle_stats_timer(void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  if (ctx->frame_stats != nullptr) {
    ctx->frame_stats->OutputStats();
  }
  wl_event_source_timer_update(ctx->stats_timer_event_source.get(),
                               ctx->stats_timer_delay);
  return 1;
}

static void sl_execvp(const char* file,
                      char* const argv[],
                      int wayland_socked_fd) {
  if (wayland_socked_fd >= 0) {
    int fd;
    fd = dup(wayland_socked_fd);
    putenv(sl_xasprintf("WAYLAND_SOCKET=%d", fd));
  }

  setenv("SOMMELIER_VERSION", SOMMELIER_VERSION, 1);

  execvp(file, argv);
  perror(file);
}

static void sl_calculate_scale_for_xwayland(struct sl_context* ctx) {
  double default_scale_factor = 1.0;
  double scale = ctx->desired_scale;

  if (!ctx->use_direct_scale) {
    // Find internal output and determine preferred scale factor.
    for (auto output : ctx->host_outputs) {
      if (output->internal) {
        double preferred_scale =
            sl_output_aura_scale_factor_to_double(output->preferred_scale);

        if (ctx->aura_shell) {
          double device_scale_factor = sl_output_aura_scale_factor_to_double(
              output->device_scale_factor);

          default_scale_factor = device_scale_factor * preferred_scale;
        }
        break;
      }
    }
    // We use the default scale factor multiplied by desired scale set by the
    // user. This gives us HiDPI support by default but the user can still
    // adjust it if higher or lower density is preferred.
    scale = ctx->desired_scale * default_scale_factor;

    // Round to integer scale if wp_viewporter interface is not present.
    if (!ctx->viewporter)
      scale = round(scale);
  }

  // Clamp and set scale.
  ctx->scale = MIN(MAX_SCALE, MAX(MIN_SCALE, scale));

  // Scale affects output state. Send updated output state to xwayland.
  for (auto output : ctx->host_outputs)
    sl_output_calculate_virtual_dimensions(output);
  sl_output_update_output_x(ctx);
  for (auto output : ctx->host_outputs)
    sl_output_send_host_output_state(output);
}

// Extra connections are made to XWayland-hosting instances for IME support.
static void sl_extra_client_created_notify(struct wl_listener* listener,
                                           void* data) {
  sl_context* ctx =
      wl_container_of(listener, ctx, extra_client_created_listener);
  wl_client* client = static_cast<wl_client*>(data);

  sl_set_display_implementation(ctx, client);
}

static int sl_handle_display_ready_event(int fd, uint32_t mask, void* data) {
  TRACE_EVENT("surface", "sl_handle_display_ready_event");
  struct sl_context* ctx = (struct sl_context*)data;
  char display_name[9];
  int bytes_read = 0;
  pid_t pid;

  if (!(mask & WL_EVENT_READABLE)) {
    LOG(FATAL) << "Got error or hangup on display ready connection (mask"
               << mask << "), exiting";
    exit(EXIT_FAILURE);
  }

  display_name[0] = ':';
  do {
    int bytes_left = sizeof(display_name) - bytes_read - 1;
    int bytes;

    if (!bytes_left)
      break;

    bytes = read(fd, &display_name[bytes_read + 1], bytes_left);
    if (!bytes)
      break;

    bytes_read += bytes;
  } while (display_name[bytes_read] != '\n');

  display_name[bytes_read] = '\0';
  setenv("DISPLAY", display_name, 1);

  sl_connect(ctx);

  ctx->display_ready_event_source.reset();
  close(fd);

  // Calculate scale now that the default scale factor is known. This also
  // happens to workaround an issue in Xwayland where an output update is
  // needed for DPI to be set correctly.
  sl_calculate_scale_for_xwayland(ctx);
  wl_display_flush_clients(ctx->host_display);

  // Open wayland socket so cros_im can provide IME support to X11 apps.
  // This depends on the display number, which is only available once Xwayland
  // initialization is complete (now), so we can't create this earlier to use
  // it for the wayland connection with Xwayland.
  char* socket_name = sl_xasprintf("DISPLAY-%s-wl", display_name);
  struct sockaddr_un addr;
  int lock_fd;
  int sock_fd;
  int rv = sl_open_wayland_socket(socket_name, &addr, &lock_fd, &sock_fd);
  if (rv >= 0) {
    wl_display_add_socket_fd(ctx->host_display, sock_fd);
    ctx->extra_client_created_listener.notify = sl_extra_client_created_notify;
    wl_display_add_client_created_listener(ctx->host_display,
                                           &ctx->extra_client_created_listener);
  } else {
    LOG(WARNING) << "unable to open wayland socket for cros_im";
  }
  free(socket_name);

  putenv(sl_xasprintf("XCURSOR_SIZE=%d",
                      static_cast<int>(XCURSOR_SIZE_BASE * ctx->scale + 0.5)));

  pid = fork();
  errno_assert(pid >= 0);
  if (pid == 0) {
    // Set WAYLAND_DISPLAY to a value that is guaranteed to not point to a
    // valid wayland compositor socket name. Unsetting WAYLAND_DISPLAY is
    // insufficient as some clients may attempt to connect to wayland-0 as a
    // default fallback.
    setenv("WAYLAND_DISPLAY", ".", 1);
    sl_execvp(ctx->runprog[0], ctx->runprog, -1);
    _exit(EXIT_FAILURE);
  }

  ctx->child_pid = pid;

  return 1;
}

static void sl_sigchld_handler(int signal) {
  while (waitpid(-1, nullptr, WNOHANG) > 0)
    continue;
}

static void sl_client_destroy_notify(struct wl_listener* listener, void* data) {
  exit(0);
}

// Break |str| into a sequence of zero or more nonempty arguments. No more
// than |argc| arguments will be added to |argv|. Returns the total number of
// argments found in |str|.
static int sl_parse_cmd_prefix(char* str, int argc, char const** argv) {
  char* s = str;
  int n = 0;
  int delim = 0;

  do {
    // Look for ending delimiter if |delim| is set.
    if (delim) {
      if (*s == delim) {
        delim = 0;
        *s = '\0';
      }
      ++s;
    } else {
      // Skip forward to first non-space character.
      while (*s == ' ' && *s != '\0')
        ++s;

      // Check for quote delimiter.
      if (*s == '"') {
        delim = '"';
        ++s;
      } else {
        delim = ' ';
      }

      // Add string to arguments if there's room.
      if (n < argc)
        argv[n] = s;

      ++n;
    }
  } while (*s != '\0');

  return n;
}

static void sl_print_usage() {
  printf(
      "usage: sommelier [options] [program] [args...]\n\n"
      "options:\n"
      "  -h, --help\t\t\tPrint this help\n"
      "  -X\t\t\t\tEnable X11 forwarding\n"
      "  --log-level=LEVEL\t\tSet minimum log level to be processed\n"
      "\t(allowed range: -1 to 3; lower is more verbose))\n"
      "  --parent\t\t\tRun as parent and spawn child processes\n"
      "  --socket=SOCKET\t\tName of socket to listen on\n"
      "  --display=DISPLAY\t\tWayland display to connect to\n"
      "  --scale=SCALE\t\t\tScale factor for contents\n"
      "  --dpi=[DPI[,DPI...]]\t\tDPI buckets\n"
      "  --peer-cmd-prefix=PREFIX\tPeer process command line prefix\n"
      "  --accelerators=ACCELERATORS\tList of keyboard accelerators\n"
      "  --windowed_accelerators=ACCELERATORS\tList of keyboard accelerators\n"
      "\tonly while windowed\n"
      "  --application-id=ID\t\tForced application ID for all X11 windows\n"
      "  --vm-identifier=NAME\t\tName of the VM, used to identify X11 "
      "windows.\n"
      "\t\t\t\tIgnored if --application-id is set.\n"
      "  --application-id-x11-property=PROPERTY\n"
      "\tA cardinal window property used to identify X11 windows, as follows:\n"
      "\t  org.chromium.<vm-identifier>.xprop.<application-id-x11-property>\n"
      "\tIgnored if --application-id is set.\n"
      "  --x-display=DISPLAY\t\tX11 display to listen on\n"
      "  --xwayland-path=PATH\t\tPath to Xwayland executable\n"
      "  --xwayland-gl-driver-path=PATH\tPath to GL drivers for Xwayland\n"
      "  --xwayland-cmd-prefix=PREFIX\tXwayland command line prefix\n"
      "  --enable-linux-dmabuf\t\tEnable client-facing zwp_linux_dmabuf_v1 "
      "extension support\n"
      "  --enable-xshape\t\tEnable X11 XShape extension support\n"
      "  --enable-x11-move-windows\t\tLet X11 apps control window position,\n"
      "\tif the host compositor supports the aura_shell protocol.\n"
      "  --no-exit-with-child\t\tKeep process alive after child exists\n"
      "  --no-clipboard-manager\tDisable X11 clipboard manager\n"
      "  --frame-color=COLOR\t\tWindow frame color for X11 clients\n"
      "  --no-support-damage-buffer\t"
      "Disable wl_surface::damage_buffer support.\n"
      "  --force-drm-device=DEVICE\tDRM device to use\n"
      "  --glamor\t\t\tUse glamor to accelerate X11 clients\n"
      "  --timing-filename=PATH\tPath to timing output log\n"
      "  --direct-scale\t\tEnable direct scaling mode\n"
      "  --viewport-resize\t\tUse viewport to resize unresizable windows.\n"
#ifdef PERFETTO_TRACING
      "  --trace-filename=PATH\t\tPath to Perfetto trace filename\n"
      "  --trace-system\t\tPerfetto trace to system daemon\n"
#endif
#ifdef QUIRKS_SUPPORT
      "  --quirks-config=PATH[,PATH...]\tOne or more 'quirks' config files.\n"
      "  --print-enabled-features=STEAM_GAME_ID\tPrint features enabled via "
      "quirks for the game specified, then exit.\n"
#endif
      "  --stats-summary=PATH\t\tPath for recent frame timing stats "
      "(rewrites)\n"
      "  --stats-log=PATH\t\t\tPath to log/append frame timing stats "
      "(appends)\n"
      "\tThe two previous arguments log the same data for different usages.\n"
      "\tThe summary is the most recent entries for the metrics collector,\n"
      "\twhile the log will grow infinitely and is intended for debugging\n"
      "\tor development purposes.\n"
      "  --stats-timer=SECS\t\tNumber of seconds between stats dumps\n"
      "  --fullscreen-mode=MODE\tDefault fullscreen behavior (immersive,"
      " plain)\n"
      "  --virtgpu-channel\t\tUse virtgpu cross-domain context as virtual"
      " wayland channel\n"
      "  --noop-driver\t\t\tPass through to existing Wayland server"
      " without virtualization\n");
}

static const char* sl_arg_value(const char* arg) {
  assert(arg != nullptr);

  const char* s = strchr(arg, '=');
  if (!s) {
    sl_print_usage();
    printf("\n");
    LOG(FATAL) << "failed to extract value from arg '" << arg
               << "'. expected form is " << arg << "=value";
    exit(EXIT_FAILURE);
  }
  return s + 1;
}

// attempts to both split "--argname=argval" and parse "argval" signed integer,
// or exits the program with a fatal error if detecting any non-integer
// characters or is out-of-bounds.
static int64_t sl_arg_parse_int_checked(const char* arg) {
  assert(arg != nullptr);
  const char* argval = sl_arg_value(arg);

  // Use strtoll() because atoi() fails with UB, not error signaling.
  // Note, that both parse and out-of-range failures are detected here,
  // unlike atoi() (which implicitly succeeds with return value 0);
  //
  // e.g. atoi() parses "blah8" as '0' and "8blah" as '8', while this procedure
  // detects invalid input in both cases and signals a fatality.
  char* str_end;
  int64_t res = strtoll(argval, &str_end, 10);

  const bool has_non_numerical = (str_end == argval || *str_end != '\0');
  const bool out_of_bounds =
      (res == LLONG_MAX || res == LLONG_MIN) && errno == ERANGE;
  if (has_non_numerical || out_of_bounds) {
    LOG(FATAL) << "failed to convert string '" << argval
               << "' to int while parsing argument '" << arg << "'";
    exit(EXIT_FAILURE);
  }
  return res;
}

// Parse the list of accelerators that should be reserved by the
// compositor. Format is "|MODIFIERS|KEYSYM", where MODIFIERS is a
// list of modifier names (E.g. <Control><Alt>) and KEYSYM is an
// XKB key symbol name (E.g Delete).
// Sommelier will exit with EXIT_FAILURE if this returns false.
// TODO(b/237946069) Confirm accelerator handling works with i18n.
static bool sl_parse_accelerators(
    std::vector<struct sl_accelerator*>& accelerator_list,
    const char* accelerators) {
  if (accelerators) {
    uint32_t modifiers = 0;

    while (*accelerators) {
      if (*accelerators == ',') {
        accelerators++;
      } else if (*accelerators == '<') {
        if (strncmp(accelerators, "<Control>", 9) == 0) {
          modifiers |= CONTROL_MASK;
          accelerators += 9;
        } else if (strncmp(accelerators, "<Alt>", 5) == 0) {
          modifiers |= ALT_MASK;
          accelerators += 5;
        } else if (strncmp(accelerators, "<Shift>", 7) == 0) {
          modifiers |= SHIFT_MASK;
          accelerators += 7;
        } else {
          LOG(ERROR) << "invalid modifier";
          return false;
        }
      } else {
        const char* end = strchrnul(accelerators, ',');
        char* name = strndup(accelerators, end - accelerators);

        struct sl_accelerator* accelerator =
            static_cast<sl_accelerator*>(malloc(sizeof(*accelerator)));
        accelerator->modifiers = modifiers;
        accelerator->symbol =
            xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE);
        if (accelerator->symbol == XKB_KEY_NoSymbol) {
          LOG(ERROR) << "invalid key symbol";
          return false;
        }

        accelerator_list.push_back(accelerator);

        modifiers = 0;
        accelerators = end;
        free(name);
      }
    }
  }
  return true;
}

// Takes the lock and then opens the socket for wayland clients to connect to.
// Returns a negative value on error. |addr|, |lock_fd| and |sock_fd| will be
// populated on success.
int sl_open_wayland_socket(const char* socket_name,
                           struct sockaddr_un* addr,
                           int* lock_fd,
                           int* sock_fd) {
  int rv;

  const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir) {
    LOG(ERROR) << "XDG_RUNTIME_DIR not set in the environment";
    return -1;
  }

  addr->sun_family = AF_LOCAL;
  snprintf(addr->sun_path, sizeof(addr->sun_path), "%s/%s", runtime_dir,
           socket_name);

  char* lock_addr = sl_xasprintf("%s%s", addr->sun_path, LOCK_SUFFIX);

  *lock_fd = open(lock_addr, O_CREAT | O_CLOEXEC,
                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
  errno_assert(*lock_fd >= 0);

  rv = flock(*lock_fd, LOCK_EX | LOCK_NB);
  if (rv < 0) {
    LOG(ERROR) << "error: unable to lock " << lock_addr
               << ", is another compositor running?";
    return rv;
  }
  free(lock_addr);

  struct stat sock_stat;
  rv = stat(addr->sun_path, &sock_stat);
  if (rv >= 0) {
    if (sock_stat.st_mode & (S_IWUSR | S_IWGRP))
      unlink(addr->sun_path);
  } else {
    errno_assert(errno == ENOENT);
  }

  *sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
  errno_assert(*sock_fd >= 0);

  rv = bind(*sock_fd, (struct sockaddr*)addr, sizeof(*addr));
  errno_assert(rv >= 0);

  rv = listen(*sock_fd, 128);
  errno_assert(rv >= 0);

  return 0;
}

int sl_run_parent(int argc,
                  char** argv,
                  sl_context* ctx,
                  const char* socket_name,
                  const char* peer_cmd_prefix) {
  struct sockaddr_un addr;
  int lock_fd;
  int sock_fd;
  int rv;

  rv = sl_open_wayland_socket(socket_name, &addr, &lock_fd, &sock_fd);
  if (rv < 0)
    return EXIT_FAILURE;

  // Spawn optional child process before we notify systemd that we're ready
  // to accept connections. WAYLAND_DISPLAY will be set but any attempt to
  // connect to this socket at this time will fail.
  if (ctx->runprog && ctx->runprog[0]) {
    int pid = fork();
    errno_assert(pid != -1);
    if (pid == 0) {
      setenv("WAYLAND_DISPLAY", socket_name, 1);
      sl_execvp(ctx->runprog[0], ctx->runprog, -1);
      _exit(EXIT_FAILURE);
    }
    while (waitpid(-1, nullptr, WNOHANG) != pid)
      continue;
  }

  if (ctx->sd_notify)
    sl_sd_notify(ctx->sd_notify);

  struct sigaction sa;
  sa.sa_handler = sl_sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  rv = sigaction(SIGCHLD, &sa, nullptr);
  errno_assert(rv >= 0);

  while (true) {
    struct ucred ucred;
    socklen_t length = sizeof(addr);

    int client_fd = accept(sock_fd, (struct sockaddr*)&addr, &length);
    if (client_fd < 0) {
      LOG(ERROR) << "failed to accept: " << strerror(errno);
      continue;
    }

    ucred.pid = -1;
    length = sizeof(ucred);
    rv = getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &length);

    int pid = fork();
    errno_assert(pid != -1);
    if (pid == 0) {
      char* client_fd_str;
      char* peer_pid_str;
      char* peer_cmd_prefix_str;
      char const* args[64];
      int i = 0;

      close(sock_fd);
      close(lock_fd);

      if (peer_cmd_prefix) {
        peer_cmd_prefix_str = sl_xasprintf("%s", peer_cmd_prefix);

        i = sl_parse_cmd_prefix(peer_cmd_prefix_str, 32, args);
        if (i > 32) {
          LOG(ERROR) << "too many arguments in cmd prefix: " << i;
          i = 0;
        }
      }

      args[i++] = argv[0];
      peer_pid_str = sl_xasprintf("--peer-pid=%d", ucred.pid);
      args[i++] = peer_pid_str;
      client_fd_str = sl_xasprintf("--client-fd=%d", client_fd);
      args[i++] = client_fd_str;

      // forward some flags.
      for (int j = 1; j < argc; ++j) {
        char* arg = argv[j];
        if (strstr(arg, "--display") == arg || strstr(arg, "--scale") == arg ||
            strstr(arg, "--direct-scale") == arg ||
            strstr(arg, "--accelerators") == arg ||
            strstr(arg, "--windowed-accelerators") == arg ||
            strstr(arg, "--drm-device") == arg ||
            strstr(arg, "--support-damage-buffer") == arg ||
            strstr(arg, "--vm-identififer") == arg ||
            strstr(arg, "--trace-system") == arg ||
            strstr(arg, "--enable-linux-dmabuf") == arg) {
          args[i++] = arg;
        }
      }

      args[i++] = nullptr;

      execvp(args[0], const_cast<char* const*>(args));
      _exit(EXIT_FAILURE);
    }
    close(client_fd);
  }

  // Control should never reach here.
  assert(false);
}

void sl_spawn_xwayland(sl_context* ctx,
                       wl_event_loop* event_loop,
                       int wayland_socket_fd,
                       const char* xwayland_cmd_prefix,
                       const char* xwayland_path,
                       int xdisplay,
                       const char* xauth_path,
                       const char* xfont_path,
                       const char* xwayland_gl_driver_path,
                       const char* glamor) {
  int ds[2];
  int rv;
  // Xwayland display ready socket.
  rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ds);
  errno_assert(!rv);

  ctx->display_ready_event_source.reset(
      wl_event_loop_add_fd(event_loop, ds[0], WL_EVENT_READABLE,
                           sl_handle_display_ready_event, ctx));

  int wm[2];
  // X connection to Xwayland.
  rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, wm);
  errno_assert(!rv);

  ctx->wm_fd = wm[0];

  int pid = fork();
  errno_assert(pid != -1);
  if (pid == 0) {
    char const* args[64];
    int i = 0;

    if (xwayland_cmd_prefix) {
      char* xwayland_cmd_prefix_str = sl_xasprintf("%s", xwayland_cmd_prefix);

      i = sl_parse_cmd_prefix(xwayland_cmd_prefix_str, 32, args);
      if (i > 32) {
        LOG(ERROR) << "too many arguments in cmd prefix: " << i;
        i = 0;
      }
    }

    args[i++] = sl_xasprintf("%s", xwayland_path ?: XWAYLAND_PATH);

    int fd = dup(ds[1]);
    char* display_fd_str = sl_xasprintf("%d", fd);
    fd = dup(wm[1]);
    char* wm_fd_str = sl_xasprintf("%d", fd);

    if (xdisplay > 0) {
      args[i++] = sl_xasprintf(":%d", xdisplay);
    }
    args[i++] = "-nolisten";
    args[i++] = "tcp";
    args[i++] = "-rootless";
    // Use software rendering unless we have a DRM device and glamor is
    // enabled.
    if (!ctx->drm_device || !glamor || !strcmp(glamor, "0"))
      args[i++] = "-shm";
    args[i++] = "-displayfd";
    args[i++] = display_fd_str;
    args[i++] = "-wm";
    args[i++] = wm_fd_str;
    if (xauth_path) {
      args[i++] = "-auth";
      args[i++] = sl_xasprintf("%s", xauth_path);
    }
    if (xfont_path) {
      args[i++] = "-fp";
      args[i++] = sl_xasprintf("%s", xfont_path);
    }
    args[i++] = nullptr;

    // If a path is explicitly specified via command line or environment
    // use that instead of the compiled in default.  In either case, only
    // set the environment variable if the value specified is non-empty.
    if (xwayland_gl_driver_path) {
      if (*xwayland_gl_driver_path) {
        setenv("LIBGL_DRIVERS_PATH", xwayland_gl_driver_path, 1);
      }
    } else if (XWAYLAND_GL_DRIVER_PATH && *XWAYLAND_GL_DRIVER_PATH) {
      setenv("LIBGL_DRIVERS_PATH", XWAYLAND_GL_DRIVER_PATH, 1);
    }

    sl_execvp(args[0], const_cast<char* const*>(args), wayland_socket_fd);
    _exit(EXIT_FAILURE);
  }
  close(wm[1]);
  ctx->xwayland_pid = pid;
}

void create_shims() {
  // xdg-shell shims.
  set_xdg_positioner_shim(new XdgPositionerShim());
  set_xdg_popup_shim(new XdgPopupShim());
  set_xdg_toplevel_shim(new XdgToplevelShim());
  set_xdg_surface_shim(new XdgSurfaceShim());
  set_xdg_wm_base_shim(new XdgWmBaseShim());
  set_wp_viewport_shim(new WpViewportShim());

#ifdef GAMEPAD_SUPPORT
  Libevdev::Set(new LibevdevShim());
#endif
}

static WaylandChannel* try_wayland_channel_init(
    WaylandChannel* channel, const char* channel_description) {
  if (channel) {
    int ret = channel->init();
    if (ret) {
      LOG(VERBOSE) << "WaylandChannel init for candidate '"
                   << channel_description << "' failed: " << strerror(-ret);
      delete channel;
      channel = nullptr;
    } else {
      LOG(VERBOSE) << "WaylandChannel init for candidate '"
                   << channel_description << "' succeeded.";
    }
  }
  return channel;
}

WaylandChannel* sl_create_wayland_channel(bool try_virtgpu_channel) {
  // defines a preferred channel ordering and attempt early init so we can
  // fallback if necessary.
  WaylandChannel* channel = nullptr;
  if (try_virtgpu_channel) {
    channel = try_wayland_channel_init(new VirtGpuChannel(), "VirtGpuChannel");
  }
  if (!channel) {
    channel = try_wayland_channel_init(new VirtWaylandChannel(),
                                       "VirtWaylandChannel");
  }
  if (!channel) {
    LOG(ERROR) << "WaylandChannel init failed for all candidate backends";
  }
  return channel;
}

int real_main(int argc, char** argv) {
  struct sl_context ctx;
  sl_context_init_default(&ctx);

  create_shims();

  const char* display = getenv("SOMMELIER_DISPLAY");
  const char* scale = getenv("SOMMELIER_SCALE");
  const char* dpi = getenv("SOMMELIER_DPI");
  const char* clipboard_manager = getenv("SOMMELIER_CLIPBOARD_MANAGER");
  const char* frame_color = getenv("SOMMELIER_FRAME_COLOR");
  const char* dark_frame_color = getenv("SOMMELIER_DARK_FRAME_COLOR");
  const char* support_damage_buffer = getenv("SOMMELIER_SUPPORT_DAMAGE_BUFFER");
  const char* force_drm_device = nullptr;
  const char* glamor = getenv("SOMMELIER_GLAMOR");
  const char* fullscreen_mode = getenv("SOMMELIER_FULLSCREEN_MODE");
  const char* peer_cmd_prefix = getenv("SOMMELIER_PEER_CMD_PREFIX");
  const char* xwayland_cmd_prefix = getenv("SOMMELIER_XWAYLAND_CMD_PREFIX");
  const char* accelerators = getenv("SOMMELIER_ACCELERATORS");
  const char* windowed_accelerators = getenv("SOMMELIER_WINDOWED_ACCELERATORS");
  const char* xwayland_path = getenv("SOMMELIER_XWAYLAND_PATH");
  const char* xwayland_gl_driver_path =
      getenv("SOMMELIER_XWAYLAND_GL_DRIVER_PATH");
  const char* xauth_path = getenv("SOMMELIER_XAUTH_PATH");
  const char* xfont_path = getenv("SOMMELIER_XFONT_PATH");
  const char* vm_id = getenv("SOMMELIER_VM_IDENTIFIER");
#ifdef QUIRKS_SUPPORT
  const char* quirks_paths = getenv("SOMMELIER_QUIRKS_CONFIG");
#endif
  const char* socket_name = "wayland-0";
  bool noop_driver = false;
  struct wl_listener client_destroy_listener = {};
  client_destroy_listener.notify = sl_client_destroy_notify;
  int sv[2];
  pid_t pid;
  int xdisplay = -1;
  int parent = 0;
  int client_fd = -1;
  int i;
  const char* stats_summary = nullptr;
  const char* stats_log = nullptr;
  bool use_virtgpu_channel = false;

  // Ignore SIGUSR1 (used for trace dumping) in all child processes.
  signal(SIGUSR1, SIG_IGN);

  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0 ||
        strcmp(arg, "-?") == 0) {
      sl_print_usage();
      return EXIT_SUCCESS;
    }
    if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("Version: %s\n", SOMMELIER_VERSION);
      return EXIT_SUCCESS;
    }
#ifdef QUIRKS_SUPPORT
    if (strstr(arg, "--print-enabled-features") == arg) {
      if (!quirks_paths) {
        LOG(FATAL) << "Env var SOMMELIER_QUIRKS_CONFIG must be set to quirks "
                      "textproto path";
        return EXIT_FAILURE;
      }
      Quirks quirks;
      quirks.LoadFromCommaSeparatedFiles(quirks_paths);

      int64_t steam_game_id = sl_arg_parse_int_checked(arg);
      if (steam_game_id <= 0) {
        LOG(FATAL) << "invalid value for arg '" << arg << "': " << steam_game_id
                   << " (expected positive integer)";
        return EXIT_FAILURE;
      }
      quirks.PrintFeaturesEnabled(steam_game_id);
      return EXIT_SUCCESS;
    }
#endif

    if (strstr(arg, "--parent") == arg) {
      parent = 1;
    } else if (strstr(arg, "--log-level") == arg) {
      const int64_t log_level = sl_arg_parse_int_checked(arg);
      logging::set_min_log_level(log_level);
    } else if (strstr(arg, "--socket") == arg) {
      socket_name = sl_arg_value(arg);
    } else if (strstr(arg, "--display") == arg) {
      display = sl_arg_value(arg);
    } else if (strstr(arg, "--peer-pid") == arg) {
      ctx.peer_pid = atoi(sl_arg_value(arg));
    } else if (strstr(arg, "--peer-cmd-prefix") == arg) {
      peer_cmd_prefix = sl_arg_value(arg);
    } else if (strstr(arg, "--xwayland-cmd-prefix") == arg) {
      xwayland_cmd_prefix = sl_arg_value(arg);
    } else if (strstr(arg, "--client-fd") == arg) {
      client_fd = atoi(sl_arg_value(arg));
    } else if (strstr(arg, "--direct-scale") == arg) {
      ctx.use_direct_scale = true;
    } else if (strstr(arg, "--scale") == arg) {
      scale = sl_arg_value(arg);
    } else if (strstr(arg, "--dpi") == arg) {
      dpi = sl_arg_value(arg);
    } else if (strstr(arg, "--accelerators") == arg) {
      accelerators = sl_arg_value(arg);
    } else if (strstr(arg, "--windowed-accelerators") == arg) {
      windowed_accelerators = sl_arg_value(arg);
    } else if (strstr(arg, "--vm-identifier") == arg) {
      vm_id = sl_arg_value(arg);
    } else if (strstr(arg, "--application-id-x11-property") == arg) {
      // NB: Must be parsed before --application-id.
      ctx.application_id_property_name = sl_arg_value(arg);
    } else if (strstr(arg, "--application-id") == arg) {
      ctx.application_id = sl_arg_value(arg);
    } else if (strstr(arg, "-X") == arg) {
      ctx.xwayland = 1;
    } else if (strstr(arg, "--x-display") == arg) {
      xdisplay = atoi(sl_arg_value(arg));
      // Automatically enable X forwarding if X display is specified.
      ctx.xwayland = 1;
    } else if (strstr(arg, "--xwayland-path") == arg) {
      xwayland_path = sl_arg_value(arg);
    } else if (strstr(arg, "--xwayland-gl-driver-path") == arg) {
      xwayland_gl_driver_path = sl_arg_value(arg);
    } else if (strstr(arg, "--no-exit-with-child") == arg) {
      ctx.exit_with_child = 0;
    } else if (strstr(arg, "--sd-notify") == arg) {
      ctx.sd_notify = sl_arg_value(arg);
    } else if (strstr(arg, "--no-clipboard-manager") == arg) {
      clipboard_manager = "0";
    } else if (strstr(arg, "--frame-color") == arg) {
      frame_color = sl_arg_value(arg);
    } else if (strstr(arg, "--dark-frame-color") == arg) {
      dark_frame_color = sl_arg_value(arg);
    } else if (strstr(arg, "--force-drm-device") == arg) {
      force_drm_device = sl_arg_value(arg);
    } else if (strstr(arg, "--no-support-damage-buffer") == arg) {
      support_damage_buffer = "0";
    } else if (strstr(arg, "--glamor") == arg) {
      glamor = "1";
    } else if (strstr(arg, "--fullscreen-mode") == arg) {
      fullscreen_mode = sl_arg_value(arg);
    } else if (strstr(arg, "--x-auth") == arg) {
      xauth_path = sl_arg_value(arg);
    } else if (strstr(arg, "--x-font-path") == arg) {
      xfont_path = sl_arg_value(arg);
    } else if (strstr(arg, "--timing-filename") == arg) {
      ctx.timing = new Timing(sl_arg_value(arg));
    } else if (strstr(arg, "--explicit-fence") == arg) {
      ctx.use_explicit_fence = true;
    } else if (strstr(arg, "--enable-linux-dmabuf") == arg) {
      ctx.enable_linux_dmabuf = true;
    } else if (strstr(arg, "--enable-xshape") == arg) {
      ctx.enable_xshape = true;
    } else if (strstr(arg, "--enable-x11-move-windows") == arg) {
      ctx.enable_x11_move_windows = true;
    } else if (strstr(arg, "--virtgpu-channel") == arg) {
      use_virtgpu_channel = true;
    } else if (strstr(arg, "--noop-driver") == arg) {
      noop_driver = true;
    } else if (strstr(arg, "--stable-scaling") == arg) {
      ctx.stable_scaling = true;
    } else if (strstr(arg, "--viewport-resize") == arg) {
      ctx.viewport_resize = true;
    } else if (strstr(arg, "--allow-xwayland-emulate-screen-pos-size") == arg) {
      ctx.allow_xwayland_emulate_screen_pos_size = true;
    } else if (strstr(arg, "--ignore-stateless-toplevel-configure") == arg) {
      ctx.ignore_stateless_toplevel_configure = true;
    } else if (strstr(arg, "--only-client-can-exit-fullscreen") == arg) {
      ctx.only_client_can_exit_fullscreen = true;
#ifdef PERFETTO_TRACING
    } else if (strstr(arg, "--trace-filename") == arg) {
      ctx.trace_filename = sl_arg_value(arg);
    } else if (strstr(arg, "--trace-system") == arg) {
      ctx.trace_system = true;
#endif
#ifdef QUIRKS_SUPPORT
    } else if (strstr(arg, "--quirks-config") == arg) {
      quirks_paths = sl_arg_value(arg);
#endif
    } else if (strstr(arg, "--stats-summary") == arg) {
      stats_summary = sl_arg_value(arg);
    } else if (strstr(arg, "--stats-log") == arg) {
      stats_log = sl_arg_value(arg);
    } else if (strstr(arg, "--stats-timer") == arg) {
      ctx.stats_timer_delay = atoi(sl_arg_value(arg)) * 1000;
    } else if (arg[0] == '-') {
      if (strcmp(arg, "--") == 0) {
        ctx.runprog = &argv[i + 1];
        break;
      }
      // Don't exit on unknown options so we can have forward compatibility
      // with new flags introduced.
      LOG(WARNING) << "option '" << arg << "' is unknown, ignoring";
    } else {
      ctx.runprog = &argv[i];
      break;
    }
  }

  if (vm_id && strlen(vm_id)) {
    // Some GuestOS instances of sommelier will be started with vm_id set to a
    // container_token from /dev/.container_token.
    // The vm_id may be empty if the token or file does not yet exist.
    // Keep the vm_id as default (termina) if this occurs.
    ctx.vm_id = vm_id;
  }

  if (ctx.application_id && ctx.vm_id) {
    LOG(WARNING) << "--application-id overrides --vm-identifier";
  }
  if (ctx.application_id && ctx.application_id_property_name) {
    LOG(WARNING) << "--application-id overrides --application-id-x11-property";
  }

  if (parent) {
    return sl_run_parent(argc, argv, &ctx, socket_name, peer_cmd_prefix);
  }

  if (client_fd == -1) {
    if (!ctx.runprog || !ctx.runprog[0]) {
      sl_print_usage();
      return EXIT_FAILURE;
    }
  }

  if (ctx.xwayland) {
    assert(client_fd == -1);

    ctx.clipboard_manager = 1;
    if (clipboard_manager)
      ctx.clipboard_manager = !!strcmp(clipboard_manager, "0");
  }

  if (scale) {
    ctx.desired_scale = atof(scale);
    // Round to integer scale until we detect wp_viewporter support.
    // In direct scale mode, take the scale value as is.

    if (ctx.use_direct_scale) {
      ctx.scale = ctx.desired_scale;
    } else {
      ctx.scale = MIN(MAX_SCALE, MAX(MIN_SCALE, round(ctx.desired_scale)));
    }
  }

  if (!frame_color)
    frame_color = FRAME_COLOR;

  if (frame_color) {
    int r, g, b;
    if (sscanf(frame_color, "#%02x%02x%02x", &r, &g, &b) == 3)
      ctx.frame_color = 0xff000000 | (r << 16) | (g << 8) | (b << 0);
  }

  if (!dark_frame_color)
    dark_frame_color = DARK_FRAME_COLOR;

  if (dark_frame_color) {
    int r, g, b;
    if (sscanf(dark_frame_color, "#%02x%02x%02x", &r, &g, &b) == 3)
      ctx.dark_frame_color = 0xff000000 | (r << 16) | (g << 8) | (b << 0);
  }

  ctx.support_damage_buffer = support_damage_buffer == nullptr ||
                              strcmp(support_damage_buffer, "1") == 0;

#ifdef QUIRKS_SUPPORT
  if (quirks_paths) {
    ctx.quirks.LoadFromCommaSeparatedFiles(quirks_paths);
  }
#endif

  if (fullscreen_mode) {
    if (strcmp(fullscreen_mode, "immersive") == 0) {
      ctx.fullscreen_mode = ZAURA_SURFACE_FULLSCREEN_MODE_IMMERSIVE;
    } else if (strcmp(fullscreen_mode, "plain") == 0) {
      ctx.fullscreen_mode = ZAURA_SURFACE_FULLSCREEN_MODE_PLAIN;
    } else {
      LOG(FATAL) << "unrecognised --fullscreen-mode";
      sl_print_usage();
      return EXIT_FAILURE;
    }
  }

  // Handle broken pipes without signals that kill the entire process.
  signal(SIGPIPE, SIG_IGN);

  wl_event_loop* event_loop = nullptr;
  {
    ScopeTimer timer("configure event loop");
    // We use a wayland virtual context unless display name was explicitly
    // specified.
    const bool use_virtual_context = !display;

    WaylandChannel* wayland_channel = nullptr;
    if (!noop_driver) {
      wayland_channel = sl_create_wayland_channel(use_virtgpu_channel);
      if (!wayland_channel) {
        LOG(FATAL) << "failed to initialize wayland channel.";
        return EXIT_FAILURE;
      }
    } else if (use_virtual_context) {
      LOG(FATAL) << "Must either provide a wayland display (--display=<name>) "
                    "or use a virtual wayland channel (remove --noop-driver).";
      return EXIT_FAILURE;
    }

    event_loop = sl_context_configure_event_loop(&ctx, wayland_channel,
                                                 use_virtual_context);
    if (!event_loop) {
      return EXIT_FAILURE;
    }
  }

  {
    ScopeTimer timer("drm device");
    int drm_fd = -1;
    char* drm_device = nullptr;
    if (force_drm_device != nullptr) {
      // Use DRM device specified on the command line.
      drm_fd = open(force_drm_device, O_RDWR | O_CLOEXEC);
      if (drm_fd == -1) {
        LOG(FATAL) << "could not open " << force_drm_device << ": "
                   << strerror(errno);
        return EXIT_FAILURE;
      }
      drm_device = strdup(force_drm_device);
    } else {
      // Enumerate render nodes to find the virtio_gpu device.
      drm_fd = open_virtgpu(&drm_device);
    }
    if (drm_fd >= 0) {
      ctx.gbm = gbm_create_device(drm_fd);
      if (!ctx.gbm) {
        LOG(FATAL) << "couldn't get display device";
        return EXIT_FAILURE;
      }

      ctx.drm_device = drm_device;
    }
  }

  wl_array_init(&ctx.dpi);
  if (dpi) {
    char* str = strdup(dpi);
    char* token = strtok(str, ",");  // NOLINT(runtime/threadsafe_fn)
    int* p;

    while (token) {
      p = static_cast<int*>(wl_array_add(&ctx.dpi, sizeof *p));
      assert(p);
      *p = MAX(MIN_DPI, MIN(atoi(token), MAX_DPI));
      token = strtok(nullptr, ",");  // NOLINT(runtime/threadsafe_fn)
    }
    free(str);
  }

  // The success of this depends on xkb-data being installed.
  ctx.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!ctx.xkb_context) {
    LOG(FATAL) << "xkb_context_new failed. xkb-data missing?";
    return EXIT_FAILURE;
  }

  {
    ScopeTimer timer("connect display");
    if (ctx.virtwl_display_fd != -1) {
      ctx.display = wl_display_connect_to_fd(ctx.virtwl_display_fd);
    } else {
      if (display == nullptr)
        display = getenv("WAYLAND_DISPLAY");
      if (display == nullptr)
        display = "wayland-0";

      ctx.display = wl_display_connect(display);
    }

    if (!ctx.display) {
      LOG(FATAL) << "failed to connect to " << display;
      return EXIT_FAILURE;
    }

    if (!sl_parse_accelerators(ctx.accelerators, accelerators))
      return EXIT_FAILURE;
    if (!sl_parse_accelerators(ctx.windowed_accelerators,
                               windowed_accelerators))
      return EXIT_FAILURE;

    ctx.display_event_source.reset(
        wl_event_loop_add_fd(event_loop, wl_display_get_fd(ctx.display),
                             WL_EVENT_READABLE, sl_handle_event, &ctx));

    wl_registry_add_listener(wl_display_get_registry(ctx.display),
                             &sl_registry_listener, &ctx);
  }

  {
    ScopeTimer timer("client create");
    if (ctx.runprog || ctx.xwayland) {
      // Wayland connection from client.
      int rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
      errno_assert(!rv);
      client_fd = sv[0];
    }

    ctx.client = wl_client_create(ctx.host_display, client_fd);
  }

  {
    ScopeTimer timer("display implementation");
    // Replace the core display implementation. This is needed in order to
    // implement sync handler properly.
    sl_set_display_implementation(&ctx, ctx.client);
  }

  {
    ScopeTimer timer("spawn xwayland");
    if (ctx.runprog || ctx.xwayland) {
      ctx.sigchld_event_source.reset(wl_event_loop_add_signal(
          event_loop, SIGCHLD, sl_handle_sigchld, &ctx));

      if (ctx.xwayland) {
        sl_spawn_xwayland(&ctx, event_loop, sv[1], xwayland_cmd_prefix,
                          xwayland_path, xdisplay, xauth_path, xfont_path,
                          xwayland_gl_driver_path, glamor);
      } else {
        pid = fork();
        errno_assert(pid != -1);
        if (pid == 0) {
          // Unset DISPLAY to prevent X clients from connecting to an existing X
          // server when X forwarding is not enabled.
          unsetenv("DISPLAY");
          sl_execvp(ctx.runprog[0], ctx.runprog, sv[1]);
          _exit(EXIT_FAILURE);
        }
        ctx.child_pid = pid;
      }
      close(sv[1]);
    }
  }
  // Attempt to enable tracing.  This could be called earlier but would rather
  // spawn all children first.
  const bool tracing_needed = ctx.trace_filename || ctx.trace_system;
  if (tracing_needed) {
    initialize_tracing(ctx.trace_filename, ctx.trace_system);
    enable_tracing(!ctx.trace_system);
  }

  // Trigger trace and timing log dumps when USR1 signals are received
  if (tracing_needed || ctx.timing) {
    ctx.sigusr1_event_source.reset(
        wl_event_loop_add_signal(event_loop, SIGUSR1, sl_handle_sigusr1, &ctx));
  }

  // Initialize timing log values.
  if (ctx.timing) {
    ctx.timing->RecordStartTime();
  }

  if (stats_summary != nullptr || stats_log != nullptr) {
    ctx.frame_stats.reset(new FrameStats(stats_summary, stats_log));
    ctx.stats_timer_event_source.reset(
        wl_event_loop_add_timer(event_loop, sl_handle_stats_timer, &ctx));
    wl_event_source_timer_update(ctx.stats_timer_event_source.get(),
                                 ctx.stats_timer_delay);
  }

  wl_client_add_destroy_listener(ctx.client, &client_destroy_listener);

  LOG(VERBOSE) << "starting main loop";
  while (true) {
    wl_display_flush_clients(ctx.host_display);
    if (ctx.connection) {
      if (ctx.needs_set_input_focus) {
        sl_set_input_focus(&ctx, ctx.host_focus_window);
        ctx.needs_set_input_focus = 0;
      }
      xcb_flush(ctx.connection);
    }
    if (wl_display_flush(ctx.display) < 0)
      return EXIT_FAILURE;

    if (wl_event_loop_dispatch(event_loop, -1) == -1) {
      // Ignore EINTR or sommelier will exit when attached by strace or gdb.
      if (errno != EINTR)
        return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}  // NOLINT(readability/fn_size)
