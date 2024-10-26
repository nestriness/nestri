// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unlike the other protocol handlers, this one talks the stylus-unstable-v2
// protocol to to the host, but exposes the tablet-unstable-v2 protocol
// to the clients.

#include "sommelier.h"                // NOLINT(build/include_directory)
#include "sommelier-transform.h"      // NOLINT(build/include_directory)
#include "sommelier-stylus-tablet.h"  // NOLINT(build/include_directory)
#include "sommelier-inpututils.h"     // NOLINT(build/include_directory)

#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "tablet-unstable-v2-server-protocol.h"  // NOLINT(build/include_directory)
#include "stylus-unstable-v2-client-protocol.h"  // NOLINT(build/include_directory)

namespace {

// Versions supported by sommelier.
constexpr uint32_t kTabletManagerVersion = 1;

}  // namespace

struct sl_host_tablet {
  struct wl_resource* tablet;
  struct wl_resource* pen_tool;
  struct wl_resource* eraser_tool;

  bool valid;
  uint32_t tool_type;
  int32_t touch_id;
  double force;
  wl_fixed_t tilt_x;
  wl_fixed_t tilt_y;
  uint32_t last_time;

  void (*destroy_cb)(void* data);
  void* data;
};

struct sl_host_stylus_tablet {
  struct sl_seat* seat;
  struct wl_touch* touch_proxy;
  struct zcr_touch_stylus_v2* stylus_proxy;
  struct sl_touchrecorder* recorder;

  struct sl_host_surface* focus_surface;
  struct wl_listener focus_resource_listener;
  struct wl_resource* focus_resource;
  struct sl_host_tablet* tablet;
};

static void sl_host_tablet_destroy(struct sl_host_tablet* tablet);

static void sl_host_tablet_tool_set_cursor(struct wl_client* client,
                                           struct wl_resource* resource,
                                           uint32_t serial,
                                           struct wl_resource* surface,
                                           int32_t hotspot_x,
                                           int32_t hotspot_y) {
  // NOOP
}

static void sl_host_tablet_tool_destroy(struct wl_client* client,
                                        struct wl_resource* tool) {
  wl_resource_destroy(tool);
}

static const struct zwp_tablet_tool_v2_interface tablet_tool_interface {
  sl_host_tablet_tool_set_cursor, sl_host_tablet_tool_destroy,
};

static void sl_host_tablet_tool_destroy(struct wl_resource* tool) {
  struct sl_host_tablet* tablet =
      static_cast<sl_host_tablet*>(wl_resource_get_user_data(tool));

  if (tool == tablet->pen_tool) {
    tablet->pen_tool = nullptr;
  } else {
    tablet->eraser_tool = nullptr;
  }

  sl_host_tablet_destroy(tablet);
}

static struct wl_resource* sl_host_tablet_tool_create(
    struct wl_client* client,
    struct wl_resource* tablet_seat,
    struct sl_host_tablet* resources,
    zwp_tablet_tool_v2_type type) {
  struct wl_resource* tool =
      wl_resource_create(client, &zwp_tablet_tool_v2_interface,
                         wl_resource_get_version(tablet_seat), 0);
  wl_resource_set_implementation(tool, &tablet_tool_interface, resources,
                                 sl_host_tablet_tool_destroy);
  zwp_tablet_seat_v2_send_tool_added(tablet_seat, tool);
  zwp_tablet_tool_v2_send_type(tool, type);
  zwp_tablet_tool_v2_send_capability(tool,
                                     ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE);
  zwp_tablet_tool_v2_send_capability(tool, ZWP_TABLET_TOOL_V2_CAPABILITY_TILT);
  zwp_tablet_tool_v2_send_done(tool);

  return tool;
}

static void sl_host_tablet_tablet_destroy(struct wl_client* client,
                                          struct wl_resource* tablet) {
  wl_resource_destroy(tablet);
}

static const struct zwp_tablet_v2_interface tablet_interface {
  sl_host_tablet_tablet_destroy,
};

static void sl_host_tablet_tablet_destroy(struct wl_resource* resource) {
  struct sl_host_tablet* tablet =
      static_cast<sl_host_tablet*>(wl_resource_get_user_data(resource));

  tablet->tablet = nullptr;

  sl_host_tablet_destroy(tablet);
}

static struct wl_resource* sl_host_tablet_tablet_create(
    struct wl_client* client,
    struct wl_resource* tablet_seat,
    struct sl_host_tablet* tablet) {
  struct wl_resource* resource =
      wl_resource_create(client, &zwp_tablet_v2_interface,
                         wl_resource_get_version(tablet_seat), 0);
  wl_resource_set_implementation(resource, &tablet_interface, tablet,
                                 sl_host_tablet_tablet_destroy);
  zwp_tablet_seat_v2_send_tablet_added(tablet_seat, resource);
  zwp_tablet_v2_send_name(resource, "Touchscreen");
  zwp_tablet_v2_send_done(resource);

  return resource;
}

static void sl_host_tablet_destroy(struct sl_host_tablet* tablet) {
  void (*destroy_cb)(void* data) = tablet->destroy_cb;
  void* data = tablet->data;

  if (tablet->pen_tool) {
    zwp_tablet_tool_v2_send_removed(tablet->pen_tool);
    wl_resource_set_destructor(tablet->pen_tool, nullptr);
    wl_resource_destroy(tablet->pen_tool);
    tablet->pen_tool = nullptr;
  }

  if (tablet->eraser_tool) {
    zwp_tablet_tool_v2_send_removed(tablet->eraser_tool);
    wl_resource_set_destructor(tablet->eraser_tool, nullptr);
    wl_resource_destroy(tablet->eraser_tool);
    tablet->eraser_tool = nullptr;
  }

  if (tablet->tablet) {
    zwp_tablet_v2_send_removed(tablet->tablet);
    sl_host_tablet_tablet_destroy(tablet->tablet);
    wl_resource_set_destructor(tablet->tablet, nullptr);
    wl_resource_destroy(tablet->tablet);
    tablet->tablet = nullptr;
  }

  delete tablet;

  destroy_cb(data);
}

static struct sl_host_tablet* sl_host_tablet_create(
    struct wl_client* client,
    struct wl_resource* tablet_seat,
    void (*destroy_cb)(void* data),
    void* data) {
  struct sl_host_tablet* tablet = new sl_host_tablet();
  tablet->tablet = sl_host_tablet_tablet_create(client, tablet_seat, tablet);
  tablet->pen_tool = sl_host_tablet_tool_create(client, tablet_seat, tablet,
                                                ZWP_TABLET_TOOL_V2_TYPE_PEN);
  tablet->eraser_tool = sl_host_tablet_tool_create(
      client, tablet_seat, tablet, ZWP_TABLET_TOOL_V2_TYPE_ERASER);

  tablet->destroy_cb = destroy_cb;
  tablet->data = data;

  return tablet;
}

static void sl_host_tablet_send_pressure_and_tilt(struct sl_host_tablet* data,
                                                  struct wl_resource* tool) {
  if (data->force) {
    uint32_t pressure = data->force * 65535.0;
    zwp_tablet_tool_v2_send_pressure(tool, pressure);
    data->force = 0.0;
  }

  if (data->tilt_x || data->tilt_y) {
    zwp_tablet_tool_v2_send_tilt(tool, data->tilt_x, data->tilt_y);
    data->tilt_x = 0;
    data->tilt_y = 0;
  }
}

static void sl_set_last_event_serial(struct wl_resource* surface_resource,
                                     uint32_t serial) {
  struct sl_host_surface* host_surface = static_cast<sl_host_surface*>(
      wl_resource_get_user_data(surface_resource));

  host_surface->last_event_serial = serial;
}

static wl_resource* sl_host_tablet_active_tool(struct sl_host_tablet* tablet) {
  if (tablet->tool_type == ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN) {
    return tablet->pen_tool;
  } else if (tablet->tool_type == ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_ERASER) {
    return tablet->eraser_tool;
  } else {
    abort();
  }
}

static void sl_touch_stylus_touch_down(void* data,
                                       struct wl_touch* wl_touch,
                                       uint32_t serial,
                                       uint32_t time,
                                       struct wl_surface* surface,
                                       int32_t id,
                                       wl_fixed_t x,
                                       wl_fixed_t y) {
  struct sl_host_surface* host_surface =
      surface ? static_cast<sl_host_surface*>(wl_surface_get_user_data(surface))
              : nullptr;
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);
  struct wl_resource* tool = sl_host_tablet_active_tool(stylus_tablet->tablet);

  wl_fixed_t ix = x;
  wl_fixed_t iy = y;

  if (!host_surface)
    return;

  if (id != stylus_tablet->tablet->touch_id)
    return;

  if (host_surface->resource != stylus_tablet->focus_resource) {
    wl_list_remove(&stylus_tablet->focus_resource_listener.link);
    wl_list_init(&stylus_tablet->focus_resource_listener.link);
    stylus_tablet->focus_resource = host_surface->resource;
    stylus_tablet->focus_surface = host_surface;
    wl_resource_add_destroy_listener(host_surface->resource,
                                     &stylus_tablet->focus_resource_listener);
  }

  if (stylus_tablet->seat->ctx->xwayland) {
    // Make sure focus surface is on top before sending down event.
    sl_restack_windows(stylus_tablet->seat->ctx,
                       wl_resource_get_id(host_surface->resource));
    sl_roundtrip(stylus_tablet->seat->ctx);
  }

  sl_transform_host_to_guest_fixed(stylus_tablet->seat->ctx, host_surface, &ix,
                                   &iy);
  zwp_tablet_tool_v2_send_proximity_in(
      tool, serial, stylus_tablet->tablet->tablet, host_surface->resource);
  zwp_tablet_tool_v2_send_motion(tool, ix, iy);
  zwp_tablet_tool_v2_send_frame(tool, time);
  zwp_tablet_tool_v2_send_down(tool, serial);
  sl_host_tablet_send_pressure_and_tilt(stylus_tablet->tablet, tool);

  if (stylus_tablet->focus_resource)
    sl_set_last_event_serial(stylus_tablet->focus_resource, serial);
  stylus_tablet->seat->last_serial = serial;
  stylus_tablet->tablet->last_time = time;
}

static void sl_touch_stylus_touch_up(void* data,
                                     struct wl_touch* wl_touch,
                                     uint32_t serial,
                                     uint32_t time,
                                     int32_t id) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);
  struct wl_resource* tool = sl_host_tablet_active_tool(stylus_tablet->tablet);

  if (id != stylus_tablet->tablet->touch_id)
    return;

  wl_list_remove(&stylus_tablet->focus_resource_listener.link);
  wl_list_init(&stylus_tablet->focus_resource_listener.link);
  stylus_tablet->focus_resource = nullptr;
  stylus_tablet->focus_surface = nullptr;

  sl_host_tablet_send_pressure_and_tilt(stylus_tablet->tablet, tool);
  zwp_tablet_tool_v2_send_up(tool);
  zwp_tablet_tool_v2_send_proximity_out(tool);
  stylus_tablet->tablet->valid = false;

  if (stylus_tablet->focus_resource)
    sl_set_last_event_serial(stylus_tablet->focus_resource, serial);
  stylus_tablet->seat->last_serial = serial;
  stylus_tablet->tablet->last_time = time;
}

static void sl_touch_stylus_touch_motion(void* data,
                                         struct wl_touch* wl_touch,
                                         uint32_t time,
                                         int32_t id,
                                         wl_fixed_t x,
                                         wl_fixed_t y) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);
  struct wl_resource* tool = sl_host_tablet_active_tool(stylus_tablet->tablet);

  wl_fixed_t ix = x;
  wl_fixed_t iy = y;

  if (id != stylus_tablet->tablet->touch_id)
    return;

  if (!stylus_tablet->focus_surface)
    return;

  sl_transform_host_to_guest_fixed(stylus_tablet->seat->ctx,
                                   stylus_tablet->focus_surface, &ix, &iy);
  sl_host_tablet_send_pressure_and_tilt(stylus_tablet->tablet, tool);
  zwp_tablet_tool_v2_send_motion(tool, ix, iy);

  stylus_tablet->tablet->last_time = time;
}

static void sl_touch_stylus_touch_frame(void* data, struct wl_touch* wl_touch) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);
  struct wl_resource* tool = sl_host_tablet_active_tool(stylus_tablet->tablet);

  sl_host_tablet_send_pressure_and_tilt(stylus_tablet->tablet, tool);
  zwp_tablet_tool_v2_send_frame(tool, stylus_tablet->tablet->last_time);
}

static void sl_touch_stylus_touch_cancel(void* data,
                                         struct wl_touch* wl_touch) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);
  struct wl_resource* tool = sl_host_tablet_active_tool(stylus_tablet->tablet);

  zwp_tablet_tool_v2_send_frame(tool, stylus_tablet->tablet->last_time);
}

static const struct wl_touch_listener sl_touch_stylus_touch_listener = {
    sl_touch_stylus_touch_down,   sl_touch_stylus_touch_up,
    sl_touch_stylus_touch_motion, sl_touch_stylus_touch_frame,
    sl_touch_stylus_touch_cancel,
};

void sl_host_stylus_tablet_handle_touch(
    struct sl_host_stylus_tablet* stylus_tablet,
    struct sl_touchrecorder* recorder) {
  if (stylus_tablet->tablet->valid) {
    sl_touchrecorder_purge_id(recorder, stylus_tablet->tablet->touch_id);
  }
}

static void sl_host_tablet_recorder_frame(void* data,
                                          struct sl_touchrecorder* recorder) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);
  if (stylus_tablet->tablet->valid) {
    sl_touchrecorder_replay_to_listener(
        recorder, &sl_touch_stylus_touch_listener, stylus_tablet);
  }
}

static void sl_touch_stylus_tool(void* data,
                                 struct zcr_touch_stylus_v2* stylus,
                                 uint32_t id,
                                 uint32_t type) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);

  if (type == ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_TOUCH) {
    return;
  }

  stylus_tablet->tablet->tool_type = type;
  stylus_tablet->tablet->touch_id = id;
  stylus_tablet->tablet->valid = true;
}

static void sl_touch_stylus_force(void* data,
                                  struct zcr_touch_stylus_v2* stylus,
                                  uint32_t time,
                                  uint32_t id,
                                  wl_fixed_t force) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);

  stylus_tablet->tablet->force = wl_fixed_to_double(force);
}

static void sl_touch_stylus_tilt(void* data,
                                 struct zcr_touch_stylus_v2* stylus,
                                 uint32_t time,
                                 uint32_t id,
                                 wl_fixed_t tilt_x,
                                 wl_fixed_t tilt_y) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);

  stylus_tablet->tablet->tilt_x = tilt_x;
  stylus_tablet->tablet->tilt_y = tilt_y;
}

static const struct zcr_touch_stylus_v2_listener
    sl_internal_touch_stylus_listener = {
        sl_touch_stylus_tool,
        sl_touch_stylus_force,
        sl_touch_stylus_tilt,
};

static void sl_tablet_focus_resource_destroyed(struct wl_listener* listener,
                                               void* data) {
  struct sl_host_stylus_tablet* host;

  host = wl_container_of(listener, host, focus_resource_listener);
  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = nullptr;
  host->focus_surface = nullptr;
}

static void sl_host_touch_tablet_destroy(void* data) {
  struct sl_host_stylus_tablet* stylus_tablet =
      static_cast<sl_host_stylus_tablet*>(data);

  zcr_touch_stylus_v2_destroy(stylus_tablet->stylus_proxy);
  wl_touch_destroy(stylus_tablet->touch_proxy);
  sl_touchrecorder_destroy(stylus_tablet->recorder);
  stylus_tablet->seat->stylus_tablet = nullptr;

  delete stylus_tablet;
}

static struct sl_host_stylus_tablet* sl_host_touch_tablet_create(
    struct sl_context* ctx,
    struct wl_client* client,
    struct sl_host_seat* seat,
    struct wl_resource* tablet_seat) {
  struct sl_host_stylus_tablet* stylus_tablet = new sl_host_stylus_tablet();

  stylus_tablet->seat = seat->seat;
  stylus_tablet->touch_proxy = wl_seat_get_touch(seat->proxy);
  stylus_tablet->recorder = sl_touchrecorder_attach(
      stylus_tablet->touch_proxy, sl_host_tablet_recorder_frame, nullptr,
      stylus_tablet);

  stylus_tablet->stylus_proxy = zcr_stylus_v2_get_touch_stylus(
      ctx->stylus_input_manager->internal, stylus_tablet->touch_proxy);
  zcr_touch_stylus_v2_add_listener(stylus_tablet->stylus_proxy,
                                   &sl_internal_touch_stylus_listener,
                                   stylus_tablet);

  wl_list_init(&stylus_tablet->focus_resource_listener.link);
  stylus_tablet->focus_resource_listener.notify =
      sl_tablet_focus_resource_destroyed;
  stylus_tablet->focus_resource = nullptr;
  stylus_tablet->focus_surface = nullptr;

  stylus_tablet->tablet = sl_host_tablet_create(
      client, tablet_seat, sl_host_touch_tablet_destroy, stylus_tablet);

  return stylus_tablet;
}

static void sl_host_tablet_manager_get_tablet_seat(
    struct wl_client* client,
    struct wl_resource* tablet_manager,
    uint32_t id,
    struct wl_resource* seat) {
  struct sl_context* ctx =
      static_cast<sl_context*>(wl_resource_get_user_data(tablet_manager));
  struct sl_host_seat* host_seat =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(seat));

  struct wl_resource* tablet_seat =
      wl_resource_create(client, &zwp_tablet_seat_v2_interface,
                         wl_resource_get_version(tablet_manager), id);

  if (host_seat->seat->stylus_tablet)
    return;

  if (ctx->stylus_input_manager && ctx->stylus_input_manager->internal) {
    host_seat->seat->stylus_tablet =
        sl_host_touch_tablet_create(ctx, client, host_seat, tablet_seat);
  }
}

static void sl_host_tablet_manager_destroy(struct wl_client* client,
                                           struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct zwp_tablet_manager_v2_interface
    sl_tablet_manager_implementation = {sl_host_tablet_manager_get_tablet_seat,
                                        sl_host_tablet_manager_destroy};

static void sl_bind_host_tablet_manager(struct wl_client* client,
                                        void* data,
                                        uint32_t app_version,
                                        uint32_t id) {
  struct sl_context* ctx = static_cast<sl_context*>(data);
  struct wl_resource* resource =
      wl_resource_create(client, &zwp_tablet_manager_v2_interface,
                         MIN(app_version, kTabletManagerVersion), id);
  wl_resource_set_implementation(resource, &sl_tablet_manager_implementation,
                                 ctx, nullptr);
}

struct sl_global* sl_stylus_to_tablet_manager_global_create(
    struct sl_context* ctx) {
  return sl_global_create(ctx, &zwp_tablet_manager_v2_interface,
                          kTabletManagerVersion, ctx,
                          sl_bind_host_tablet_manager);
}
