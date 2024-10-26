// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"                // NOLINT(build/include_directory)
#include "sommelier-inpututils.h"     // NOLINT(build/include_directory)
#include "sommelier-stylus-tablet.h"  // NOLINT(build/include_directory)
#include "sommelier-transform.h"      // NOLINT(build/include_directory)

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "keyboard-extension-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)

struct sl_host_keyboard {
  struct sl_seat* seat;
  struct wl_resource* resource;
  struct wl_keyboard* proxy;
  struct zcr_extended_keyboard_v1* extended_keyboard_proxy;
  struct wl_resource* focus_resource;
  struct wl_listener focus_resource_listener;
  uint32_t focus_serial;
  struct xkb_keymap* keymap;
  struct xkb_state* state;
  xkb_mod_mask_t control_mask;
  xkb_mod_mask_t alt_mask;
  xkb_mod_mask_t shift_mask;
  uint32_t modifiers;
  struct wl_array pressed_keys;
};

struct sl_host_touch {
  struct sl_seat* seat;
  struct wl_resource* resource;
  struct wl_touch* proxy;
  struct wl_resource* focus_resource;
  struct sl_host_surface* focus_surface;
  struct wl_listener focus_resource_listener;
  // TODO(b/281760854): This is needed for translating stylus to tablet events.
  struct sl_touchrecorder* recorder;
};

static void sl_host_pointer_set_cursor(struct wl_client* client,
                                       struct wl_resource* resource,
                                       uint32_t serial,
                                       struct wl_resource* surface_resource,
                                       int32_t hotspot_x,
                                       int32_t hotspot_y) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_resource_get_user_data(resource));
  struct sl_host_surface* host_surface = nullptr;

  int32_t hsx = hotspot_x;
  int32_t hsy = hotspot_y;

  if (surface_resource) {
    host_surface = static_cast<sl_host_surface*>(
        wl_resource_get_user_data(surface_resource));
    host_surface->has_role = 1;
    if (host_surface->contents_width && host_surface->contents_height)
      wl_surface_commit(host_surface->proxy);
  }

  sl_transform_guest_to_host(host->seat->ctx, nullptr, &hsx, &hsy);

  wl_pointer_set_cursor(host->proxy, serial,
                        host_surface ? host_surface->proxy : nullptr, hsx, hsy);
}

static void sl_host_pointer_release(struct wl_client* client,
                                    struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct wl_pointer_interface sl_pointer_implementation = {
    sl_host_pointer_set_cursor, sl_host_pointer_release};

static void sl_set_last_event_serial(struct wl_resource* surface_resource,
                                     uint32_t serial) {
  struct sl_host_surface* host_surface = static_cast<sl_host_surface*>(
      wl_resource_get_user_data(surface_resource));

  host_surface->last_event_serial = serial;
}

static void sl_pointer_set_focus(struct sl_host_pointer* host,
                                 uint32_t serial,
                                 struct sl_host_surface* host_surface,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  struct wl_resource* surface_resource =
      host_surface ? host_surface->resource : nullptr;
  wl_fixed_t ix = x;
  wl_fixed_t iy = y;

  if (surface_resource == host->focus_resource)
    return;

  if (host->focus_resource)
    wl_pointer_send_leave(host->resource, serial, host->focus_resource);

  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = surface_resource;
  host->focus_serial = serial;
  host->focus_surface = host_surface;

  if (surface_resource) {
    if (host->seat->ctx->xwayland) {
      // Make sure focus surface is on top before sending enter event.
      sl_restack_windows(host->seat->ctx, wl_resource_get_id(surface_resource));
      sl_roundtrip(host->seat->ctx);
    }

    wl_resource_add_destroy_listener(surface_resource,
                                     &host->focus_resource_listener);

    sl_transform_host_to_guest_fixed(host->seat->ctx, host_surface, &ix, &iy);
    wl_pointer_send_enter(host->resource, serial, surface_resource, ix, iy);
  }
}

static void sl_pointer_enter(void* data,
                             struct wl_pointer* pointer,
                             uint32_t serial,
                             struct wl_surface* surface,
                             wl_fixed_t x,
                             wl_fixed_t y) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));
  struct sl_host_surface* host_surface =
      surface ? static_cast<sl_host_surface*>(wl_surface_get_user_data(surface))
              : nullptr;

  if (!host_surface)
    return;

  sl_pointer_set_focus(host, serial, host_surface, x, y);

  if (host->focus_resource)
    sl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void sl_pointer_leave(void* data,
                             struct wl_pointer* pointer,
                             uint32_t serial,
                             struct wl_surface* surface) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));

  sl_pointer_set_focus(host, serial, nullptr, 0, 0);
}

static void sl_pointer_motion(void* data,
                              struct wl_pointer* pointer,
                              uint32_t time,
                              wl_fixed_t x,
                              wl_fixed_t y) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));

  wl_fixed_t mx = x;
  wl_fixed_t my = y;

  sl_transform_pointer(host->seat->ctx, host->focus_surface, &mx, &my);
  wl_pointer_send_motion(host->resource, time, mx, my);
}

static void sl_pointer_button(void* data,
                              struct wl_pointer* pointer,
                              uint32_t serial,
                              uint32_t time,
                              uint32_t button,
                              uint32_t state) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));

  wl_pointer_send_button(host->resource, serial, time, button, state);

  if (host->focus_resource)
    sl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void sl_pointer_axis(void* data,
                            struct wl_pointer* pointer,
                            uint32_t time,
                            uint32_t axis,
                            wl_fixed_t value) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));
  wl_fixed_t svalue = value;

  sl_transform_host_to_guest_fixed(host->seat->ctx, nullptr, &svalue, axis);

  host->time = time;
  host->axis_delta[axis] += svalue;
}

static void sl_pointer_frame(void* data, struct wl_pointer* pointer) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));

  // Many X apps (e.g. VS Code, Firefox, Chromium) only allow scrolls to happen
  // in multiples of 5 units. This value comes from the smooth scrolling
  // extension of X, which says that 5 smooth scroll units is equal to 1 tick of
  // discrete scrolling.
  //
  // To avoid the experience of scrolling and seeing nothing happen, we replace
  // scroll amounts < 5 with 5 units, for discrete scrolls for X apps
  // only. Other clients should continue to see smaller scrolls as they can
  // handle them correctly, and for non-discrete scrolling (such as touch-pads)
  // we don't want to do this because it would lead to erratic jumps.
  const int kDiscreteScrollUnit = 5;

  for (int axis = 0; axis < 2; axis++) {
    if (host->axis_discrete[axis] != 0) {
      wl_pointer_send_axis_discrete(host->resource, axis,
                                    host->axis_discrete[axis]);

      double axis_delta = wl_fixed_to_double(host->axis_delta[axis]);
      if (fabs(axis_delta) < kDiscreteScrollUnit && host->seat->ctx->xwayland) {
        axis_delta = copysign(kDiscreteScrollUnit, axis_delta);
        host->axis_delta[axis] = wl_fixed_from_double(axis_delta);
      }
    }
    if (host->axis_delta[axis] != 0) {
      wl_pointer_send_axis(host->resource, host->time, axis,
                           host->axis_delta[axis]);
    }

    host->axis_delta[axis] = wl_fixed_from_int(0);
    host->axis_discrete[axis] = 0;
  }

  wl_pointer_send_frame(host->resource);
}

void sl_pointer_axis_source(void* data,
                            struct wl_pointer* pointer,
                            uint32_t axis_source) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));

  wl_pointer_send_axis_source(host->resource, axis_source);
}

static void sl_pointer_axis_stop(void* data,
                                 struct wl_pointer* pointer,
                                 uint32_t time,
                                 uint32_t axis) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));

  wl_pointer_send_axis_stop(host->resource, time, axis);
}

static void sl_pointer_axis_discrete(void* data,
                                     struct wl_pointer* pointer,
                                     uint32_t axis,
                                     int32_t discrete) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_pointer_get_user_data(pointer));

  host->axis_discrete[axis] += discrete;
}

static const struct wl_pointer_listener sl_pointer_listener = {
    sl_pointer_enter,       sl_pointer_leave,     sl_pointer_motion,
    sl_pointer_button,      sl_pointer_axis,      sl_pointer_frame,
    sl_pointer_axis_source, sl_pointer_axis_stop, sl_pointer_axis_discrete};

static void sl_host_keyboard_release(struct wl_client* client,
                                     struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct wl_keyboard_interface sl_keyboard_implementation = {
    sl_host_keyboard_release};

static void sl_keyboard_keymap(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t format,
                               int32_t fd,
                               uint32_t size) {
  struct sl_host_keyboard* host =
      static_cast<sl_host_keyboard*>(wl_keyboard_get_user_data(keyboard));

  wl_keyboard_send_keymap(host->resource, format, fd, size);

  if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    void* data = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);

    assert(data != MAP_FAILED);

    if (host->keymap)
      xkb_keymap_unref(host->keymap);

    host->keymap = xkb_keymap_new_from_string(
        host->seat->ctx->xkb_context, static_cast<char*>(data),
        XKB_KEYMAP_FORMAT_TEXT_V1,
        static_cast<xkb_keymap_compile_flags>(XKB_KEYMAP_COMPILE_NO_FLAGS));
    assert(host->keymap);

    munmap(data, size);

    if (host->state)
      xkb_state_unref(host->state);
    host->state = xkb_state_new(host->keymap);
    assert(host->state);

    host->control_mask = 1 << xkb_keymap_mod_get_index(host->keymap, "Control");
    host->alt_mask = 1 << xkb_keymap_mod_get_index(host->keymap, "Mod1");
    host->shift_mask = 1 << xkb_keymap_mod_get_index(host->keymap, "Shift");
  }

  close(fd);
}

static void sl_keyboard_set_focus(struct sl_host_keyboard* host,
                                  uint32_t serial,
                                  struct sl_host_surface* host_surface,
                                  struct wl_array* keys) {
  struct wl_resource* surface_resource =
      host_surface ? host_surface->resource : nullptr;

  if (surface_resource == host->focus_resource)
    return;

  if (host->focus_resource)
    wl_keyboard_send_leave(host->resource, serial, host->focus_resource);

  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = surface_resource;
  host->focus_serial = serial;

  if (surface_resource) {
    wl_resource_add_destroy_listener(surface_resource,
                                     &host->focus_resource_listener);
    wl_keyboard_send_enter(host->resource, serial, surface_resource, keys);
  }

  host->seat->last_serial = serial;
}

static void sl_keyboard_enter(void* data,
                              struct wl_keyboard* keyboard,
                              uint32_t serial,
                              struct wl_surface* surface,
                              struct wl_array* keys) {
  struct sl_host_keyboard* host =
      static_cast<sl_host_keyboard*>(wl_keyboard_get_user_data(keyboard));
  struct sl_host_surface* host_surface =
      surface ? static_cast<sl_host_surface*>(wl_surface_get_user_data(surface))
              : nullptr;

  if (!host_surface)
    return;

  wl_array_copy(&host->pressed_keys, keys);
  sl_keyboard_set_focus(host, serial, host_surface, keys);

  host->seat->last_serial = serial;
}

static void sl_keyboard_leave(void* data,
                              struct wl_keyboard* keyboard,
                              uint32_t serial,
                              struct wl_surface* surface) {
  struct sl_host_keyboard* host =
      static_cast<sl_host_keyboard*>(wl_keyboard_get_user_data(keyboard));
  struct wl_array array;

  wl_array_init(&array);
  sl_keyboard_set_focus(host, serial, nullptr, &array);
}

static int sl_array_set_add(struct wl_array* array, uint32_t key) {
  uint32_t* k;

  sl_array_for_each(k, array) {
    if (*k == key)
      return 0;
  }
  k = static_cast<uint32_t*>(wl_array_add(array, sizeof(key)));
  assert(k);
  *k = key;
  return 1;
}

static int sl_array_set_remove(struct wl_array* array, uint32_t key) {
  uint32_t* k;

  sl_array_for_each(k, array) {
    if (*k == key) {
      uint32_t* end = reinterpret_cast<uint32_t*>(
          reinterpret_cast<char*>(array->data) + array->size);

      *k = *(end - 1);
      array->size -= sizeof(*k);
      return 1;
    }
  }
  return 0;
}

static void sl_keyboard_key(void* data,
                            struct wl_keyboard* keyboard,
                            uint32_t serial,
                            uint32_t time,
                            uint32_t key,
                            uint32_t state) {
  struct sl_host_keyboard* host =
      static_cast<sl_host_keyboard*>(wl_keyboard_get_user_data(keyboard));
  bool handled = true;

  if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    if (host->state) {
      const xkb_keysym_t* symbols;
      uint32_t num_symbols;
      xkb_keysym_t symbol = XKB_KEY_NoSymbol;
      uint32_t code = key + 8;

      num_symbols = xkb_state_key_get_syms(host->state, code, &symbols);
      if (num_symbols == 1)
        symbol = symbols[0];

      for (auto accelerator : host->seat->ctx->accelerators) {
        if (host->modifiers == accelerator->modifiers &&
            xkb_keysym_to_lower(symbol) == accelerator->symbol) {
          handled = false;
          break;
        }
      }
      if (host->seat->ctx->host_focus_window &&
          !(host->seat->ctx->host_focus_window->fullscreen ||
            host->seat->ctx->host_focus_window->compositor_fullscreen)) {
        for (auto accelerator : host->seat->ctx->accelerators) {
          if (host->modifiers == accelerator->modifiers &&
              xkb_keysym_to_lower(symbol) == accelerator->symbol) {
            handled = false;
            break;
          }
        }
      }
    }
    // Forward key pressed event if it should be handled and not
    // already pressed.
    if (handled) {
      if (sl_array_set_add(&host->pressed_keys, key))
        wl_keyboard_send_key(host->resource, serial, time, key, state);
    }
  } else {
    // Forward key release event if currently pressed.
    handled = sl_array_set_remove(&host->pressed_keys, key);
    if (handled)
      wl_keyboard_send_key(host->resource, serial, time, key, state);
  }

  if (host->focus_resource)
    sl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;

  if (host->extended_keyboard_proxy) {
    zcr_extended_keyboard_v1_ack_key(
        host->extended_keyboard_proxy, serial,
        handled ? ZCR_EXTENDED_KEYBOARD_V1_HANDLED_STATE_HANDLED
                : ZCR_EXTENDED_KEYBOARD_V1_HANDLED_STATE_NOT_HANDLED);
  }
}

static void sl_keyboard_modifiers(void* data,
                                  struct wl_keyboard* keyboard,
                                  uint32_t serial,
                                  uint32_t mods_depressed,
                                  uint32_t mods_latched,
                                  uint32_t mods_locked,
                                  uint32_t group) {
  struct sl_host_keyboard* host =
      static_cast<sl_host_keyboard*>(wl_keyboard_get_user_data(keyboard));
  xkb_mod_mask_t mask;

  wl_keyboard_send_modifiers(host->resource, serial, mods_depressed,
                             mods_latched, mods_locked, group);

  if (host->focus_resource)
    sl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;

  if (!host->keymap)
    return;

  xkb_state_update_mask(host->state, mods_depressed, mods_latched, mods_locked,
                        0, 0, group);
  mask = xkb_state_serialize_mods(
      host->state, static_cast<xkb_state_component>(XKB_STATE_MODS_DEPRESSED |
                                                    XKB_STATE_MODS_LATCHED));
  host->modifiers = 0;
  if (mask & host->control_mask)
    host->modifiers |= CONTROL_MASK;
  if (mask & host->alt_mask)
    host->modifiers |= ALT_MASK;
  if (mask & host->shift_mask)
    host->modifiers |= SHIFT_MASK;
}

static void sl_keyboard_repeat_info(void* data,
                                    struct wl_keyboard* keyboard,
                                    int32_t rate,
                                    int32_t delay) {
  struct sl_host_keyboard* host =
      static_cast<sl_host_keyboard*>(wl_keyboard_get_user_data(keyboard));

  wl_keyboard_send_repeat_info(host->resource, rate, delay);
}

static const struct wl_keyboard_listener sl_keyboard_listener = {
    sl_keyboard_keymap, sl_keyboard_enter,     sl_keyboard_leave,
    sl_keyboard_key,    sl_keyboard_modifiers, sl_keyboard_repeat_info};

static void sl_host_touch_release(struct wl_client* client,
                                  struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct wl_touch_interface sl_touch_implementation = {
    sl_host_touch_release};

static void sl_host_touch_down(void* data,
                               struct wl_touch* touch,
                               uint32_t serial,
                               uint32_t time,
                               struct wl_surface* surface,
                               int32_t id,
                               wl_fixed_t x,
                               wl_fixed_t y) {
  struct sl_host_touch* host = static_cast<sl_host_touch*>(data);
  struct sl_host_surface* host_surface =
      surface ? static_cast<sl_host_surface*>(wl_surface_get_user_data(surface))
              : nullptr;

  wl_fixed_t ix = x;
  wl_fixed_t iy = y;

  if (!host_surface)
    return;

  if (host_surface->resource != host->focus_resource) {
    wl_list_remove(&host->focus_resource_listener.link);
    wl_list_init(&host->focus_resource_listener.link);
    host->focus_resource = host_surface->resource;
    host->focus_surface = host_surface;
    wl_resource_add_destroy_listener(host_surface->resource,
                                     &host->focus_resource_listener);
  }

  if (host->seat->ctx->xwayland) {
    // Make sure focus surface is on top before sending down event.
    sl_restack_windows(host->seat->ctx,
                       wl_resource_get_id(host_surface->resource));
    sl_roundtrip(host->seat->ctx);
  }

  sl_transform_host_to_guest_fixed(host->seat->ctx, host_surface, &ix, &iy);
  wl_touch_send_down(host->resource, serial, time, host_surface->resource, id,
                     ix, iy);

  if (host->focus_resource)
    sl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void sl_host_touch_up(void* data,
                             struct wl_touch* touch,
                             uint32_t serial,
                             uint32_t time,
                             int32_t id) {
  struct sl_host_touch* host = static_cast<sl_host_touch*>(data);

  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = nullptr;
  host->focus_surface = nullptr;

  wl_touch_send_up(host->resource, serial, time, id);

  if (host->focus_resource)
    sl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void sl_host_touch_motion(void* data,
                                 struct wl_touch* touch,
                                 uint32_t time,
                                 int32_t id,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  struct sl_host_touch* host = static_cast<sl_host_touch*>(data);
  wl_fixed_t ix = x;
  wl_fixed_t iy = y;

  sl_transform_host_to_guest_fixed(host->seat->ctx, host->focus_surface, &ix,
                                   &iy);
  wl_touch_send_motion(host->resource, time, id, ix, iy);
}

static void sl_host_touch_frame(void* data, struct wl_touch* touch) {
  struct sl_host_touch* host = static_cast<sl_host_touch*>(data);

  wl_touch_send_frame(host->resource);
}

static void sl_host_touch_cancel(void* data, struct wl_touch* touch) {
  struct sl_host_touch* host = static_cast<sl_host_touch*>(data);

  wl_touch_send_cancel(host->resource);
}

static const struct wl_touch_listener sl_touch_listener = {
    sl_host_touch_down, sl_host_touch_up, sl_host_touch_motion,
    sl_host_touch_frame, sl_host_touch_cancel};

static void sl_host_touch_recorder_frame(void* data,
                                         struct sl_touchrecorder* recorder) {
  struct sl_host_touch* host = static_cast<sl_host_touch*>(data);

  if (host->seat->stylus_tablet) {
    sl_host_stylus_tablet_handle_touch(host->seat->stylus_tablet, recorder);
  }

  sl_touchrecorder_replay_to_listener(recorder, &sl_touch_listener, data);
}

static void sl_host_touch_recorder_cancel(void* data,
                                          struct sl_touchrecorder* recorder) {
  struct sl_host_touch* host = static_cast<sl_host_touch*>(data);

  wl_touch_send_cancel(host->resource);
}

static void sl_destroy_host_pointer(struct wl_resource* resource) {
  struct sl_host_pointer* host =
      static_cast<sl_host_pointer*>(wl_resource_get_user_data(resource));

  if (wl_pointer_get_version(host->proxy) >= WL_POINTER_RELEASE_SINCE_VERSION) {
    wl_pointer_release(host->proxy);
  } else {
    wl_pointer_destroy(host->proxy);
  }
  wl_list_remove(&host->focus_resource_listener.link);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_pointer_focus_resource_destroyed(struct wl_listener* listener,
                                                void* data) {
  struct sl_host_pointer* host;

  host = wl_container_of(listener, host, focus_resource_listener);
  sl_pointer_set_focus(host, host->focus_serial, nullptr, 0, 0);
}

static void sl_host_seat_get_host_pointer(struct wl_client* client,
                                          struct wl_resource* resource,
                                          uint32_t id) {
  struct sl_host_seat* host =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(resource));
  struct sl_host_pointer* host_pointer = new sl_host_pointer();

  host_pointer->seat = host->seat;
  host_pointer->resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_pointer->resource,
                                 &sl_pointer_implementation, host_pointer,
                                 sl_destroy_host_pointer);
  host_pointer->proxy = wl_seat_get_pointer(host->proxy);
  wl_pointer_add_listener(host_pointer->proxy, &sl_pointer_listener,
                          host_pointer);
  wl_list_init(&host_pointer->focus_resource_listener.link);
  host_pointer->focus_resource_listener.notify =
      sl_pointer_focus_resource_destroyed;
  host_pointer->focus_resource = nullptr;
  host_pointer->focus_surface = nullptr;
  host_pointer->focus_serial = 0;
  host_pointer->time = 0;
  host_pointer->axis_delta[0] = wl_fixed_from_int(0);
  host_pointer->axis_delta[1] = wl_fixed_from_int(0);
  host_pointer->axis_discrete[0] = 0;
  host_pointer->axis_discrete[1] = 0;
}

static void sl_destroy_host_keyboard(struct wl_resource* resource) {
  struct sl_host_keyboard* host =
      static_cast<sl_host_keyboard*>(wl_resource_get_user_data(resource));

  if (host->extended_keyboard_proxy)
    zcr_extended_keyboard_v1_destroy(host->extended_keyboard_proxy);

  wl_array_release(&host->pressed_keys);
  if (host->keymap)
    xkb_keymap_unref(host->keymap);
  if (host->state)
    xkb_state_unref(host->state);

  if (wl_keyboard_get_version(host->proxy) >=
      WL_KEYBOARD_RELEASE_SINCE_VERSION) {
    wl_keyboard_release(host->proxy);
  } else {
    wl_keyboard_destroy(host->proxy);
  }

  wl_list_remove(&host->focus_resource_listener.link);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_keyboard_focus_resource_destroyed(struct wl_listener* listener,
                                                 void* data) {
  struct sl_host_keyboard* host;
  struct wl_array array;

  host = wl_container_of(listener, host, focus_resource_listener);
  wl_array_init(&array);
  sl_keyboard_set_focus(host, host->focus_serial, nullptr, &array);
}

static void sl_host_seat_get_host_keyboard(struct wl_client* client,
                                           struct wl_resource* resource,
                                           uint32_t id) {
  struct sl_host_seat* host =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(resource));
  struct sl_host_keyboard* host_keyboard = new sl_host_keyboard();

  host_keyboard->seat = host->seat;
  host_keyboard->resource = wl_resource_create(
      client, &wl_keyboard_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_keyboard->resource,
                                 &sl_keyboard_implementation, host_keyboard,
                                 sl_destroy_host_keyboard);
  host_keyboard->proxy = wl_seat_get_keyboard(host->proxy);
  wl_keyboard_add_listener(host_keyboard->proxy, &sl_keyboard_listener,
                           host_keyboard);
  wl_list_init(&host_keyboard->focus_resource_listener.link);
  host_keyboard->focus_resource_listener.notify =
      sl_keyboard_focus_resource_destroyed;
  host_keyboard->focus_resource = nullptr;
  host_keyboard->focus_serial = 0;
  host_keyboard->keymap = nullptr;
  host_keyboard->state = nullptr;
  host_keyboard->control_mask = 0;
  host_keyboard->alt_mask = 0;
  host_keyboard->shift_mask = 0;
  host_keyboard->modifiers = 0;
  wl_array_init(&host_keyboard->pressed_keys);

  if (host->seat->ctx->keyboard_extension) {
    host_keyboard->extended_keyboard_proxy =
        zcr_keyboard_extension_v1_get_extended_keyboard(
            host->seat->ctx->keyboard_extension->internal,
            host_keyboard->proxy);
  } else {
    host_keyboard->extended_keyboard_proxy = nullptr;
  }
}

static void sl_destroy_host_touch(struct wl_resource* resource) {
  struct sl_host_touch* host =
      static_cast<sl_host_touch*>(wl_resource_get_user_data(resource));

  if (wl_touch_get_version(host->proxy) >= WL_TOUCH_RELEASE_SINCE_VERSION) {
    wl_touch_release(host->proxy);
  } else {
    wl_touch_destroy(host->proxy);
  }

  sl_touchrecorder_destroy(host->recorder);

  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_touch_focus_resource_destroyed(struct wl_listener* listener,
                                              void* data) {
  struct sl_host_touch* host;

  host = wl_container_of(listener, host, focus_resource_listener);
  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = nullptr;
  host->focus_surface = nullptr;
}

static void sl_host_seat_get_host_touch(struct wl_client* client,
                                        struct wl_resource* resource,
                                        uint32_t id) {
  struct sl_host_seat* host =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(resource));
  struct sl_host_touch* host_touch = new sl_host_touch();

  host_touch->seat = host->seat;
  host_touch->resource = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_touch->resource, &sl_touch_implementation,
                                 host_touch, sl_destroy_host_touch);
  host_touch->proxy = wl_seat_get_touch(host->proxy);
  host_touch->recorder =
      sl_touchrecorder_attach(host_touch->proxy, sl_host_touch_recorder_frame,
                              sl_host_touch_recorder_cancel, host_touch);
  wl_list_init(&host_touch->focus_resource_listener.link);
  host_touch->focus_resource_listener.notify =
      sl_touch_focus_resource_destroyed;
  host_touch->focus_resource = nullptr;
  host_touch->focus_surface = nullptr;
}

static const struct wl_seat_interface sl_seat_implementation = {
    sl_host_seat_get_host_pointer, sl_host_seat_get_host_keyboard,
    sl_host_seat_get_host_touch, ForwardRequest<wl_seat_release>};

static void sl_seat_capabilities(void* data,
                                 struct wl_seat* seat,
                                 uint32_t capabilities) {
  struct sl_host_seat* host =
      static_cast<sl_host_seat*>(wl_seat_get_user_data(seat));

  wl_seat_send_capabilities(host->resource, capabilities);
}

static void sl_seat_name(void* data, struct wl_seat* seat, const char* name) {
  struct sl_host_seat* host =
      static_cast<sl_host_seat*>(wl_seat_get_user_data(seat));

  if (wl_resource_get_version(host->resource) >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(host->resource, name);
}

static const struct wl_seat_listener sl_seat_listener = {sl_seat_capabilities,
                                                         sl_seat_name};

static void sl_destroy_host_seat(struct wl_resource* resource) {
  struct sl_host_seat* host =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(resource));

  sl_host_seat_removed(host);

  if (wl_seat_get_version(host->proxy) >= WL_SEAT_RELEASE_SINCE_VERSION)
    wl_seat_release(host->proxy);
  else
    wl_seat_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_bind_host_seat(struct wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  struct sl_seat* seat = (struct sl_seat*)data;
  struct sl_host_seat* host = new sl_host_seat();
  host->seat = seat;
  host->resource = wl_resource_create(client, &wl_seat_interface,
                                      MIN(version, seat->version), id);
  wl_resource_set_implementation(host->resource, &sl_seat_implementation, host,
                                 sl_destroy_host_seat);
  host->proxy = static_cast<wl_seat*>(wl_registry_bind(
      wl_display_get_registry(seat->ctx->display), seat->id, &wl_seat_interface,
      wl_resource_get_version(host->resource)));
  wl_seat_add_listener(host->proxy, &sl_seat_listener, host);

  sl_host_seat_added(host);
}

struct sl_global* sl_seat_global_create(struct sl_seat* seat) {
  return sl_global_create(seat->ctx, &wl_seat_interface, seat->version, seat,
                          sl_bind_host_seat);
}
