// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_TRACING_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_TRACING_H_

#if defined(PERFETTO_TRACING)
#include <perfetto.h>
#include <xcb/xcb.h>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("surface").SetDescription(
        "Events for Wayland surface management"),
    perfetto::Category("display").SetDescription("Events for Wayland display"),
    perfetto::Category("drm").SetDescription("Events for Wayland drm"),
    perfetto::Category("shell").SetDescription("Events for Wayland shell"),
    perfetto::Category("shm").SetDescription(
        "Events for Wayland shared memory"),
    perfetto::Category("viewport")
        .SetDescription("Events for Wayland viewport"),
    perfetto::Category("sync").SetDescription("Events for Wayland sync points"),
    perfetto::Category("x11wm").SetDescription(
        "Events for X11 window management"),
    perfetto::Category("gaming").SetDescription("Events for Gaming"),
    perfetto::Category("timing").SetDescription("Events for Timing"),
    perfetto::Category("other").SetDescription("Uncategorized Wayland calls."));

void perfetto_annotate_atom(struct sl_context* ctx,
                            const perfetto::EventContext& perfetto,
                            const char* event_name,
                            xcb_atom_t atom);
void perfetto_annotate_xcb_property_state(const perfetto::EventContext& event,
                                          const char* name,
                                          uint32_t state);
void perfetto_annotate_window(struct sl_context* ctx,
                              const perfetto::EventContext& perfetto,
                              const char* event_name,
                              xcb_window_t window_id);
void perfetto_annotate_size_hints(const perfetto::EventContext& perfetto,
                                  const struct sl_wm_size_hints& size_hints);
void perfetto_annotate_cardinal_list(const perfetto::EventContext& perfetto,
                                     const char* event_name,
                                     xcb_get_property_reply_t* reply);
void perfetto_annotate_time_sync(const perfetto::EventContext& perfetto);

#else
#define TRACE_EVENT(category, name, ...)
#endif

void initialize_tracing(bool in_process_backend, bool system_backend);
void enable_tracing(bool create_session);
void dump_trace(char const* filename);
#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_TRACING_H_
