// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_INPUTUTILS_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_INPUTUTILS_H_

#include <sys/types.h>
#include <wayland-client.h>
#include <wayland-util.h>

enum sl_touchrecorder_event_type {
  SL_TOUCHRECORDER_EVENT_NONE = 0,
  SL_TOUCHRECORDER_EVENT_DOWN,
  SL_TOUCHRECORDER_EVENT_UP,
  SL_TOUCHRECORDER_EVENT_MOTION,
  SL_TOUCHRECORDER_EVENT_FRAME,
  SL_TOUCHRECORDER_EVENT_CANCEL,
};

struct sl_touchrecorder_event {
  sl_touchrecorder_event_type type;
  uint32_t serial;
  uint32_t time;
  struct wl_surface* surface;
  int32_t id;
  wl_fixed_t x;
  wl_fixed_t y;
};

struct sl_touchrecorder;

typedef void(sl_touchrecorder_frame_cb)(void* data,
                                        struct sl_touchrecorder* recorder);
typedef void(sl_touchrecorder_cancel_cb)(void* data,
                                         struct sl_touchrecorder* recorder);

struct sl_touchrecorder* sl_touchrecorder_attach(
    struct wl_touch* proxy,
    sl_touchrecorder_frame_cb* frame_cb,
    sl_touchrecorder_cancel_cb cancel_cb,
    void* data);

void sl_touchrecorder_destroy(struct sl_touchrecorder* recorder);

void sl_touchrecorder_replay_to_listener(struct sl_touchrecorder* recorder,
                                         const struct wl_touch_listener* listen,
                                         void* data);
void sl_touchrecorder_purge_id(struct sl_touchrecorder* recorder, int32_t id);

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_INPUTUTILS_H_
