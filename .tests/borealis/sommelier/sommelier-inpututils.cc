// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"             // NOLINT(build/include_directory)
#include "sommelier-inpututils.h"  // NOLINT(build/include_directory)
#include "sommelier-transform.h"   // NOLINT(build/include_directory)

#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-util.h>

struct sl_touchrecorder {
  struct wl_touch* proxy;
  sl_touchrecorder_frame_cb* frame_cb;
  sl_touchrecorder_cancel_cb* cancel_cb;
  void* data;
  unsigned events_size;
  unsigned events_alloc;
  struct sl_touchrecorder_event events[32];
};

static void sl_touchrecorder_reset(struct sl_touchrecorder* recorder) {
  recorder->events_size = 0;
}

static void sl_touchrecorder_record_event(struct sl_touchrecorder* recorder,
                                          enum sl_touchrecorder_event_type type,
                                          uint32_t serial,
                                          uint32_t time,
                                          struct wl_surface* surface,
                                          int32_t id,
                                          wl_fixed_t x,
                                          wl_fixed_t y) {
  unsigned index = recorder->events_size;

  if (index < recorder->events_alloc) {
    struct sl_touchrecorder_event* event = &recorder->events[index];

    if (index == recorder->events_alloc - 1) {
      if (type == SL_TOUCHRECORDER_EVENT_FRAME ||
          type == SL_TOUCHRECORDER_EVENT_CANCEL) {
        event->type = type;
        recorder->events_size++;
      }
    } else {
      event->type = type;
      event->serial = serial;
      event->time = time;
      event->surface = surface;
      event->id = id;
      event->x = x;
      event->y = y;

      recorder->events_size++;
    }
  }

  if (type == SL_TOUCHRECORDER_EVENT_FRAME) {
    if (recorder->frame_cb)
      recorder->frame_cb(recorder->data, recorder);
    sl_touchrecorder_reset(recorder);
  } else if (type == SL_TOUCHRECORDER_EVENT_CANCEL) {
    if (recorder->cancel_cb)
      recorder->cancel_cb(recorder->data, recorder);
    sl_touchrecorder_reset(recorder);
  }
}

static void sl_touchrecorder_down(void* data,
                                  struct wl_touch* wl_touch,
                                  uint32_t serial,
                                  uint32_t time,
                                  struct wl_surface* surface,
                                  int32_t id,
                                  wl_fixed_t x,
                                  wl_fixed_t y) {
  struct sl_touchrecorder* recorder = static_cast<sl_touchrecorder*>(data);

  sl_touchrecorder_record_event(recorder, SL_TOUCHRECORDER_EVENT_DOWN, serial,
                                time, surface, id, x, y);
}

static void sl_touchrecorder_up(void* data,
                                struct wl_touch* wl_touch,
                                uint32_t serial,
                                uint32_t time,
                                int32_t id) {
  struct sl_touchrecorder* recorder = static_cast<sl_touchrecorder*>(data);

  sl_touchrecorder_record_event(recorder, SL_TOUCHRECORDER_EVENT_UP, serial,
                                time, nullptr, id, 0, 0);
}

static void sl_touchrecorder_motion(void* data,
                                    struct wl_touch* wl_touch,
                                    uint32_t time,
                                    int32_t id,
                                    wl_fixed_t x,
                                    wl_fixed_t y) {
  struct sl_touchrecorder* recorder = static_cast<sl_touchrecorder*>(data);

  sl_touchrecorder_record_event(recorder, SL_TOUCHRECORDER_EVENT_MOTION, 0,
                                time, nullptr, id, x, y);
}

static void sl_touchrecorder_frame(void* data, struct wl_touch* wl_touch) {
  struct sl_touchrecorder* recorder = static_cast<sl_touchrecorder*>(data);

  sl_touchrecorder_record_event(recorder, SL_TOUCHRECORDER_EVENT_FRAME, 0, 0,
                                nullptr, 0, 0, 0);
}

static void sl_touchrecorder_cancel(void* data, struct wl_touch* wl_touch) {
  struct sl_touchrecorder* recorder = static_cast<sl_touchrecorder*>(data);

  sl_touchrecorder_record_event(recorder, SL_TOUCHRECORDER_EVENT_CANCEL, 0, 0,
                                nullptr, 0, 0, 0);
}

static const struct wl_touch_listener sl_touchrecorder_listener = {
    sl_touchrecorder_down, sl_touchrecorder_up, sl_touchrecorder_motion,
    sl_touchrecorder_frame, sl_touchrecorder_cancel};

struct sl_touchrecorder* sl_touchrecorder_attach(
    struct wl_touch* proxy,
    sl_touchrecorder_frame_cb* frame_cb,
    sl_touchrecorder_cancel_cb* cancel_cb,
    void* data) {
  struct sl_touchrecorder* recorder = new sl_touchrecorder();

  recorder->proxy = proxy;
  recorder->frame_cb = frame_cb;
  recorder->cancel_cb = cancel_cb;
  recorder->data = data;
  recorder->events_size = 0;
  recorder->events_alloc =
      sizeof(recorder->events) / sizeof(recorder->events[0]);
  wl_touch_add_listener(proxy, &sl_touchrecorder_listener, recorder);

  return recorder;
}

void sl_touchrecorder_destroy(struct sl_touchrecorder* recorder) {
  delete recorder;
}

void sl_touchrecorder_replay_to_listener(struct sl_touchrecorder* recorder,
                                         const struct wl_touch_listener* listen,
                                         void* data) {
  for (unsigned i = 0; i < recorder->events_size; i++) {
    struct sl_touchrecorder_event* event = &recorder->events[i];

    switch (event->type) {
      case SL_TOUCHRECORDER_EVENT_NONE:
        break;

      case SL_TOUCHRECORDER_EVENT_DOWN:
        listen->down(data, recorder->proxy, event->serial, event->time,
                     event->surface, event->id, event->x, event->y);
        break;

      case SL_TOUCHRECORDER_EVENT_UP:
        listen->up(data, recorder->proxy, event->serial, event->time,
                   event->id);
        break;

      case SL_TOUCHRECORDER_EVENT_MOTION:
        listen->motion(data, recorder->proxy, event->time, event->id, event->x,
                       event->y);
        break;

      case SL_TOUCHRECORDER_EVENT_FRAME:
        listen->frame(data, recorder->proxy);
        break;

      case SL_TOUCHRECORDER_EVENT_CANCEL:
        listen->cancel(data, recorder->proxy);
        break;

      default:
        abort();
    }
  }
}

void sl_touchrecorder_purge_id(struct sl_touchrecorder* recorder, int32_t id) {
  for (unsigned i = 0; i < recorder->events_size; i++) {
    struct sl_touchrecorder_event* event = &recorder->events[i];

    if (event->type < SL_TOUCHRECORDER_EVENT_FRAME && event->id == id) {
      event->type = SL_TOUCHRECORDER_EVENT_NONE;
    }
  }
}
