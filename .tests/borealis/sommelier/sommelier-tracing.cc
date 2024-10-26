// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-tracing.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <fcntl.h>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <vector>
#include <xcb/xproto.h>

#include "sommelier.h"          // NOLINT(build/include_directory)
#include "sommelier-ctx.h"      // NOLINT(build/include_directory)
#include "sommelier-logging.h"  // NOLINT(build/include_directory)

#if defined(_M_IA64) || defined(_M_IX86) || defined(__ia64__) ||      \
    defined(__i386__) || defined(__amd64__) || defined(__x86_64__) || \
    defined(_M_AMD64)
#define HAS_RDTSC
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif
#if defined(_M_ARM) || defined(_M_ARMT) || defined(__arm__) || \
    defined(__thumb__) || defined(__aarch64__)
// TODO(jbates): support ARM CPU counter
#endif

#if defined(PERFETTO_TRACING)
PERFETTO_TRACK_EVENT_STATIC_STORAGE();

std::unique_ptr<perfetto::TracingSession> tracing_session;

void initialize_tracing(bool in_process_backend, bool system_backend) {
  perfetto::TracingInitArgs args;
  if (in_process_backend) {
    args.backends |= perfetto::kInProcessBackend;
  }
  if (system_backend) {
    args.backends |= perfetto::kSystemBackend;
  }

  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();
}

void enable_tracing(bool create_session) {
  perfetto::TraceConfig cfg;
  cfg.add_buffers()->set_size_kb(1024);  // Record up to 1 MiB.
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");

  if (create_session) {
    tracing_session = perfetto::Tracing::NewTrace();
    tracing_session->Setup(cfg);
    tracing_session->StartBlocking();
  }
}

void dump_trace(const char* trace_filename) {
  if (!trace_filename || !*trace_filename || !tracing_session) {
    return;
  }

  std::vector<char> trace_data(tracing_session->ReadTraceBlocking());

  // Write the trace into a file.
  int fd = open(trace_filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    LOG(ERROR) << "unable to open trace file " << trace_filename << ": "
               << strerror(errno);
    return;
  }
  size_t pos = 0;
  do {
    ssize_t ret = write(fd, &trace_data[pos], trace_data.size() - pos);
    if (ret < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      }
      LOG(ERROR) << "unable to write trace file " << trace_filename << ": "
                 << strerror(errno);
      close(fd);
      return;
    }
    pos += ret;
  } while (pos < trace_data.size());
  close(fd);
}

// Returns nullptr if atom is not recognized.
static const char* xcb_atom_to_string(uint32_t atom) {
  switch (atom) {
    case XCB_ATOM_NONE:
      return "XCB_ATOM_NONE";
    case XCB_ATOM_PRIMARY:
      return "XCB_ATOM_PRIMARY";
    case XCB_ATOM_SECONDARY:
      return "XCB_ATOM_SECONDARY";
    case XCB_ATOM_ARC:
      return "XCB_ATOM_ARC";
    case XCB_ATOM_ATOM:
      return "XCB_ATOM_ATOM";
    case XCB_ATOM_BITMAP:
      return "XCB_ATOM_BITMAP";
    case XCB_ATOM_CARDINAL:
      return "XCB_ATOM_CARDINAL";
    case XCB_ATOM_COLORMAP:
      return "XCB_ATOM_COLORMAP";
    case XCB_ATOM_CURSOR:
      return "XCB_ATOM_CURSOR";
    case XCB_ATOM_CUT_BUFFER0:
      return "XCB_ATOM_CUT_BUFFER0";
    case XCB_ATOM_CUT_BUFFER1:
      return "XCB_ATOM_CUT_BUFFER1";
    case XCB_ATOM_CUT_BUFFER2:
      return "XCB_ATOM_CUT_BUFFER2";
    case XCB_ATOM_CUT_BUFFER3:
      return "XCB_ATOM_CUT_BUFFER3";
    case XCB_ATOM_CUT_BUFFER4:
      return "XCB_ATOM_CUT_BUFFER4";
    case XCB_ATOM_CUT_BUFFER5:
      return "XCB_ATOM_CUT_BUFFER5";
    case XCB_ATOM_CUT_BUFFER6:
      return "XCB_ATOM_CUT_BUFFER6";
    case XCB_ATOM_CUT_BUFFER7:
      return "XCB_ATOM_CUT_BUFFER7";
    case XCB_ATOM_DRAWABLE:
      return "XCB_ATOM_DRAWABLE";
    case XCB_ATOM_FONT:
      return "XCB_ATOM_FONT";
    case XCB_ATOM_INTEGER:
      return "XCB_ATOM_INTEGER";
    case XCB_ATOM_PIXMAP:
      return "XCB_ATOM_PIXMAP";
    case XCB_ATOM_POINT:
      return "XCB_ATOM_POINT";
    case XCB_ATOM_RECTANGLE:
      return "XCB_ATOM_RECTANGLE";
    case XCB_ATOM_RESOURCE_MANAGER:
      return "XCB_ATOM_RESOURCE_MANAGER";
    case XCB_ATOM_RGB_COLOR_MAP:
      return "XCB_ATOM_RGB_COLOR_MAP";
    case XCB_ATOM_RGB_BEST_MAP:
      return "XCB_ATOM_RGB_BEST_MAP";
    case XCB_ATOM_RGB_BLUE_MAP:
      return "XCB_ATOM_RGB_BLUE_MAP";
    case XCB_ATOM_RGB_DEFAULT_MAP:
      return "XCB_ATOM_RGB_DEFAULT_MAP";
    case XCB_ATOM_RGB_GRAY_MAP:
      return "XCB_ATOM_RGB_GRAY_MAP";
    case XCB_ATOM_RGB_GREEN_MAP:
      return "XCB_ATOM_RGB_GREEN_MAP";
    case XCB_ATOM_RGB_RED_MAP:
      return "XCB_ATOM_RGB_RED_MAP";
    case XCB_ATOM_STRING:
      return "XCB_ATOM_STRING";
    case XCB_ATOM_VISUALID:
      return "XCB_ATOM_VISUALID";
    case XCB_ATOM_WINDOW:
      return "XCB_ATOM_WINDOW";
    case XCB_ATOM_WM_COMMAND:
      return "XCB_ATOM_WM_COMMAND";
    case XCB_ATOM_WM_HINTS:
      return "XCB_ATOM_WM_HINTS";
    case XCB_ATOM_WM_CLIENT_MACHINE:
      return "XCB_ATOM_WM_CLIENT_MACHINE";
    case XCB_ATOM_WM_ICON_NAME:
      return "XCB_ATOM_WM_ICON_NAME";
    case XCB_ATOM_WM_ICON_SIZE:
      return "XCB_ATOM_WM_ICON_SIZE";
    case XCB_ATOM_WM_NAME:
      return "XCB_ATOM_WM_NAME";
    case XCB_ATOM_WM_NORMAL_HINTS:
      return "XCB_ATOM_WM_NORMAL_HINTS";
    case XCB_ATOM_WM_SIZE_HINTS:
      return "XCB_ATOM_WM_SIZE_HINTS";
    case XCB_ATOM_WM_ZOOM_HINTS:
      return "XCB_ATOM_WM_ZOOM_HINTS";
    case XCB_ATOM_MIN_SPACE:
      return "XCB_ATOM_MIN_SPACE";
    case XCB_ATOM_NORM_SPACE:
      return "XCB_ATOM_NORM_SPACE";
    case XCB_ATOM_MAX_SPACE:
      return "XCB_ATOM_MAX_SPACE";
    case XCB_ATOM_END_SPACE:
      return "XCB_ATOM_END_SPACE";
    case XCB_ATOM_SUPERSCRIPT_X:
      return "XCB_ATOM_SUPERSCRIPT_X";
    case XCB_ATOM_SUPERSCRIPT_Y:
      return "XCB_ATOM_SUPERSCRIPT_Y";
    case XCB_ATOM_SUBSCRIPT_X:
      return "XCB_ATOM_SUBSCRIPT_X";
    case XCB_ATOM_SUBSCRIPT_Y:
      return "XCB_ATOM_SUBSCRIPT_Y";
    case XCB_ATOM_UNDERLINE_POSITION:
      return "XCB_ATOM_UNDERLINE_POSITION";
    case XCB_ATOM_UNDERLINE_THICKNESS:
      return "XCB_ATOM_UNDERLINE_THICKNESS";
    case XCB_ATOM_STRIKEOUT_ASCENT:
      return "XCB_ATOM_STRIKEOUT_ASCENT";
    case XCB_ATOM_STRIKEOUT_DESCENT:
      return "XCB_ATOM_STRIKEOUT_DESCENT";
    case XCB_ATOM_ITALIC_ANGLE:
      return "XCB_ATOM_ITALIC_ANGLE";
    case XCB_ATOM_X_HEIGHT:
      return "XCB_ATOM_X_HEIGHT";
    case XCB_ATOM_QUAD_WIDTH:
      return "XCB_ATOM_QUAD_WIDTH";
    case XCB_ATOM_WEIGHT:
      return "XCB_ATOM_WEIGHT";
    case XCB_ATOM_POINT_SIZE:
      return "XCB_ATOM_POINT_SIZE";
    case XCB_ATOM_RESOLUTION:
      return "XCB_ATOM_RESOLUTION";
    case XCB_ATOM_COPYRIGHT:
      return "XCB_ATOM_COPYRIGHT";
    case XCB_ATOM_NOTICE:
      return "XCB_ATOM_NOTICE";
    case XCB_ATOM_FONT_NAME:
      return "XCB_ATOM_FONT_NAME";
    case XCB_ATOM_FAMILY_NAME:
      return "XCB_ATOM_FAMILY_NAME";
    case XCB_ATOM_FULL_NAME:
      return "XCB_ATOM_FULL_NAME";
    case XCB_ATOM_CAP_HEIGHT:
      return "XCB_ATOM_CAP_HEIGHT";
    case XCB_ATOM_WM_CLASS:
      return "XCB_ATOM_WM_CLASS";
    case XCB_ATOM_WM_TRANSIENT_FOR:
      return "XCB_ATOM_WM_TRANSIENT_FOR";
    default:
      return nullptr;
  }
}

// Annotate with the string representation of an atom.
//
// Supports well-known XCB atoms, and the fetched sl_context::atoms list. (To
// add an atom you're interested in debugging, modify |sl_context_atom_name|.)
void perfetto_annotate_atom(struct sl_context* ctx,
                            const perfetto::EventContext& perfetto,
                            const char* event_name,
                            xcb_atom_t atom) {
  auto* dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name(event_name);

  // Quickest option is to look up the XCB atoms, which have static values.
  const char* atom_str = xcb_atom_to_string(atom);
  if (atom_str) {
    dbg->set_string_value(atom_str, strlen(atom_str));
    return;
  }

  // Failing that, check if we've fetched this atom.
  for (unsigned i = 0; i < ARRAY_SIZE(ctx->atoms); ++i) {
    if (atom == ctx->atoms[i].value) {
      const char* name = sl_context_atom_name(i);
      if (name != nullptr) {
        dbg->set_string_value(name, strlen(name));
        return;
      }
    }
  }

  // If we reach here, we didn't find the atom name.
  // We could ask the X server but that would require a round-trip.
  std::string unknown("<unknown atom #");
  unknown += std::to_string(atom);
  unknown += '>';
  dbg->set_string_value(unknown);
}

void perfetto_annotate_xcb_property_state(const perfetto::EventContext& event,
                                          const char* name,
                                          unsigned int state) {
  auto* dbg = event.event()->add_debug_annotations();
  dbg->set_name(name);
  if (state == XCB_PROPERTY_NEW_VALUE) {
    static const std::string prop_new("XCB_PROPERTY_NEW_VALUE");
    dbg->set_string_value(prop_new);
  } else if (state == XCB_PROPERTY_DELETE) {
    static const std::string prop_delete("XCB_PROPERTY_DELETE");
    dbg->set_string_value(prop_delete);
  } else {
    static const std::string unknown("<unknown>");
    dbg->set_string_value(unknown);
  }
}

// Annotate the given Perfetto EventContext with the name (if known) and the ID
// of the given window.
//
// Slow (iterates a linked list); only intended to be called if tracing is
// enabled.
void perfetto_annotate_window(struct sl_context* ctx,
                              const perfetto::EventContext& perfetto,
                              const char* event_name,
                              xcb_window_t window_id) {
  auto* dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name(event_name);
  struct sl_window* window = sl_lookup_window(ctx, window_id);
  std::ostringstream value;
  if (window != nullptr && window->name != nullptr) {
    value << window->name << " <window #";
  } else {
    value << "<unknown window #";
  }
  // Always append the ID so we can track windows across their lifecycle.
  value << window_id << '>';
  dbg->set_string_value(value.str());
}

void perfetto_annotate_size_hints(const perfetto::EventContext& perfetto,
                                  const sl_wm_size_hints& size_hints) {
  auto* dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.flags");
  std::string flags;
  if (size_hints.flags & US_POSITION)
    flags += "US_POSITION|";
  if (size_hints.flags & US_SIZE)
    flags += "US_SIZE|";
  if (size_hints.flags & P_POSITION)
    flags += "P_POSITION|";
  if (size_hints.flags & P_SIZE)
    flags += "P_SIZE|";
  if (size_hints.flags & P_MIN_SIZE)
    flags += "P_MIN_SIZE|";
  if (size_hints.flags & P_MAX_SIZE)
    flags += "P_MAX_SIZE|";
  if (size_hints.flags & P_RESIZE_INC)
    flags += "P_RESIZE_INC|";
  if (size_hints.flags & P_ASPECT)
    flags += "P_ASPECT|";
  if (size_hints.flags & P_BASE_SIZE)
    flags += "P_BASE_SIZE|";
  if (size_hints.flags & P_WIN_GRAVITY)
    flags += "P_WIN_GRAVITY|";
  if (!flags.empty())
    flags.pop_back();  // remove trailing '|'
  dbg->set_string_value(flags);

  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.x");
  dbg->set_int_value(size_hints.x);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.y");
  dbg->set_int_value(size_hints.y);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.width");
  dbg->set_int_value(size_hints.width);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.height");
  dbg->set_int_value(size_hints.height);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.min_width");
  dbg->set_int_value(size_hints.min_width);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.min_height");
  dbg->set_int_value(size_hints.min_height);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.max_width");
  dbg->set_int_value(size_hints.max_width);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.max_height");
  dbg->set_int_value(size_hints.max_height);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.width_inc");
  dbg->set_int_value(size_hints.width_inc);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.height_inc");
  dbg->set_int_value(size_hints.height_inc);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.min_aspect.x");
  dbg->set_int_value(size_hints.min_aspect.x);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.min_aspect.y");
  dbg->set_int_value(size_hints.min_aspect.y);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.max_aspect.x");
  dbg->set_int_value(size_hints.max_aspect.x);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.max_aspect.y");
  dbg->set_int_value(size_hints.max_aspect.y);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.base_width");
  dbg->set_int_value(size_hints.base_width);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.base_height");
  dbg->set_int_value(size_hints.base_height);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("size_hints.win_gravity");
  dbg->set_int_value(size_hints.win_gravity);
}

// Add a Perfetto annotation for an X property storing a list of cardinals.
void perfetto_annotate_cardinal_list(const perfetto::EventContext& perfetto,
                                     const char* event_name,
                                     xcb_get_property_reply_t* reply) {
  auto* dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name(event_name);

  if (reply == nullptr) {
    static const std::string null_str("<null>");
    dbg->set_string_value(null_str);
    return;
  }

  uint32_t length = xcb_get_property_value_length(reply);
  if (length % sizeof(uint32_t) != 0) {
    static const std::string invalid("<invalid>");
    dbg->set_string_value(invalid);
    return;
  }

  uint32_t* val = static_cast<uint32_t*>(xcb_get_property_value(reply));
  uint32_t items = length / sizeof(uint32_t);
  if (items == 0) {
    static const std::string empty("<empty>");
    dbg->set_string_value(empty);
    return;
  }

  std::ostringstream str;
  str << '[' << val[0];
  for (uint32_t i = 1; i < items; ++i) {
    str << ", " << val[i];
  }
  str << ']';
  dbg->set_string_value(str.str());
}

static inline uint64_t get_cpu_ticks() {
#ifdef HAS_RDTSC
  return __rdtsc();
#else
  return 0;
#endif
}

static inline uint64_t get_timestamp_ns(clockid_t cid) {
  struct timespec ts = {};
  clock_gettime(cid, &ts);
  return static_cast<uint64_t>(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

void perfetto_annotate_time_sync(const perfetto::EventContext& perfetto) {
  uint64_t boot_time = get_timestamp_ns(CLOCK_BOOTTIME);
  uint64_t cpu_time = get_cpu_ticks();
  uint64_t monotonic_time = get_timestamp_ns(CLOCK_MONOTONIC);
  // Read again to avoid cache miss overhead.
  boot_time = get_timestamp_ns(CLOCK_BOOTTIME);
  cpu_time = get_cpu_ticks();
  monotonic_time = get_timestamp_ns(CLOCK_MONOTONIC);

  auto* dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("clock_sync_boottime");
  dbg->set_uint_value(boot_time);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("clock_sync_monotonic");
  dbg->set_uint_value(monotonic_time);
  dbg = perfetto.event()->add_debug_annotations();
  dbg->set_name("clock_sync_cputime");
  dbg->set_uint_value(cpu_time);
}

#else

// Stubs.

void initialize_tracing(bool in_process_backend, bool system_backend) {}

void enable_tracing(bool create_session) {}

void dump_trace(const char* trace_filename) {}

#endif  // PERFETTO_TRACING
