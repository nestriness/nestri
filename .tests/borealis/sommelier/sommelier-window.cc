// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-window.h"  // NOLINT(build/include_directory)

#include <algorithm>
#include <climits>
#include <fstream>
#include <assert.h>
#include <cstdint>
#include <wayland-client-protocol.h>

#include "sommelier.h"            // NOLINT(build/include_directory)
#include "sommelier-logging.h"    // NOLINT(build/include_directory)
#include "sommelier-tracing.h"    // NOLINT(build/include_directory)
#include "sommelier-transform.h"  // NOLINT(build/include_directory)
#include "viewporter-shim.h"      // NOLINT(build/include_directory)
#include "xcb/xcb-shim.h"

#include "aura-shell-client-protocol.h"  // NOLINT(build/include_directory)
#include "viewporter-client-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-shell-client-protocol.h"   // NOLINT(build/include_directory)

#ifdef QUIRKS_SUPPORT
#include "quirks/sommelier-quirks.h"
#endif

#define XID_APPLICATION_ID_FORMAT APPLICATION_ID_FORMAT_PREFIX ".xid.%d"
#define WM_CLIENT_LEADER_APPLICATION_ID_FORMAT \
  APPLICATION_ID_FORMAT_PREFIX ".wmclientleader.%d"
#define WM_CLASS_APPLICATION_ID_FORMAT \
  APPLICATION_ID_FORMAT_PREFIX ".wmclass.%s"
#define X11_PROPERTY_APPLICATION_ID_FORMAT \
  APPLICATION_ID_FORMAT_PREFIX ".xprop.%s"
sl_window::sl_window(struct sl_context* ctx,
                     xcb_window_t id,
                     int x,
                     int y,
                     int width,
                     int height,
                     int border_width)
    : ctx(ctx),
      id(id),
      x(x),
      y(y),
      width(width),
      height(height),
      border_width(border_width) {
  wl_list_insert(&ctx->unpaired_windows, &link);
  pixman_region32_init(&shape_rectangles);
}

sl_window::~sl_window() {
  if (this == ctx->host_focus_window) {
    ctx->host_focus_window = nullptr;
    ctx->needs_set_input_focus = 1;
  }

  free(name);
  free(clazz);
  free(startup_id);
  wl_list_remove(&link);
  pixman_region32_fini(&shape_rectangles);
}

void sl_configure_window(struct sl_window* window) {
  TRACE_EVENT("surface", "sl_configure_window", "id", window->id);
  assert(!window->pending_config.serial);

  if (window->next_config.mask) {
    int x = window->x;
    int y = window->y;
    int i = 0;

    xcb()->configure_window(window->ctx->connection, window->frame_id,
                            window->next_config.mask,
                            window->next_config.values);

    if (window->next_config.mask & XCB_CONFIG_WINDOW_X)
      x = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_Y)
      y = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_WIDTH)
      window->width = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_HEIGHT)
      window->height = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
      window->border_width = window->next_config.values[i++];

    // Set x/y to origin in case window gravity is not northwest as expected.
    assert(window->managed);

    uint32_t values[5];
    // Populate values[0~3] with x,y,w,h
    // x and y need to be 0. We are configuring the child window, not the frame
    // window, and the child window should always have a zero offset from its
    // frame (parent) window. See
    // https://en.wikipedia.org/wiki/Re-parenting_window_manager
    values[0] = 0;
    values[1] = 0;
    sl_window_get_width_height(window, &values[2], &values[3]);

    values[4] = window->border_width;
    xcb()->configure_window(
        window->ctx->connection, window->id,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
        values);

    if (x != window->x || y != window->y) {
      window->x = x;
      window->y = y;
      sl_send_configure_notify(window);
    }
  }

  if (window->managed) {
    xcb()->change_property(
        window->ctx->connection, XCB_PROP_MODE_REPLACE, window->id,
        window->ctx->atoms[ATOM_NET_WM_STATE].value, XCB_ATOM_ATOM, 32,
        window->next_config.states_length, window->next_config.states);
  }

  window->pending_config = window->next_config;
  window->next_config.serial = 0;
  window->next_config.mask = 0;
  window->next_config.states_length = 0;
}

void sl_send_configure_notify(struct sl_window* window) {
  // Send a "synthetic" ConfigureNotify event.
  xcb_configure_notify_event_t event = {};
  event.response_type = XCB_CONFIGURE_NOTIFY;
  event.pad0 = 0;
  event.event = window->id;
  event.window = window->id;
  event.above_sibling = XCB_WINDOW_NONE;

  // Per ICCCM, synthetic ConfigureNotify events use root coordinates
  // even if the window has been reparented.
  uint32_t xywh[4];
  sl_window_get_x_y(window, &xywh[0], &xywh[1]);
  sl_window_get_width_height(window, &xywh[2], &xywh[3]);
  event.x = static_cast<int16_t>(xywh[0]);
  event.y = static_cast<int16_t>(xywh[1]);
  event.width = static_cast<uint16_t>(xywh[2]);
  event.height = static_cast<uint16_t>(xywh[3]);

  event.border_width = static_cast<uint16_t>(window->border_width);
  event.override_redirect = 0;
  event.pad1 = 0;

  xcb()->send_event(window->ctx->connection, 0, window->id,
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                    reinterpret_cast<char*>(&event));
}

int sl_process_pending_configure_acks(struct sl_window* window,
                                      struct sl_host_surface* host_surface) {
  if (!window->pending_config.serial)
    return 0;

#ifdef COMMIT_LOOP_FIX
  // Do not commit/ack if there is nothing to change.
  //
  // TODO(b/181077580): we should never do this, but avoiding it requires a
  // more systemic fix
  if (!window->pending_config.mask && window->pending_config.states_length == 0)
    return 0;
#endif

  if (window->managed && host_surface) {
    if (window->viewport_override) {
      // If viewport override is set, only check viewport size and completely
      // ignore window size for containerized windows.
      if (sl_window_is_containerized(window) &&
          (window->viewport_height != window->viewport_height_realized ||
           window->viewport_width != window->viewport_width_realized)) {
        return 0;
      }
    } else {
      uint32_t width = window->width + window->border_width * 2;
      uint32_t height = window->height + window->border_width * 2;
      // Early out if we expect contents to match window size at some point in
      // the future.
      if ((width != host_surface->contents_width ||
           height != host_surface->contents_height)) {
        return 0;
      }
    }
  }

  if (window->xdg_surface) {
    xdg_surface_ack_configure(window->xdg_surface,
                              window->pending_config.serial);
  }
  window->pending_config.serial = 0;

  if (window->next_config.serial)
    sl_configure_window(window);

  return 1;
}

void sl_commit(struct sl_window* window, struct sl_host_surface* host_surface) {
  if (sl_process_pending_configure_acks(window, host_surface)) {
    if (host_surface)
      wl_surface_commit(host_surface->proxy);
  }
}

static const struct xdg_popup_listener sl_internal_xdg_popup_listener = {
    /*configure=*/DoNothing, /*done=*/DoNothing};

static void sl_internal_xdg_surface_configure(void* data,
                                              struct xdg_surface* xdg_surface,
                                              uint32_t serial) {
  TRACE_EVENT("surface", "sl_internal_xdg_surface_configure");
  struct sl_window* window =
      static_cast<sl_window*>(xdg_surface_get_user_data(xdg_surface));

  window->next_config.serial = serial;

  if (window->configure_event_barrier) {
    window->coalesced_next_config = window->next_config;
    window->next_config.serial = 0;
  } else if (!window->pending_config.serial) {
    struct wl_resource* host_resource;
    struct sl_host_surface* host_surface = nullptr;

    host_resource =
        wl_client_get_object(window->ctx->client, window->host_surface_id);
    if (host_resource)
      host_surface = static_cast<sl_host_surface*>(
          wl_resource_get_user_data(host_resource));

    sl_configure_window(window);
    sl_commit(window, host_surface);
  }
}

static const struct xdg_surface_listener sl_internal_xdg_surface_listener = {
    sl_internal_xdg_surface_configure};

static void sl_internal_xdg_surface_configure_barrier_done(
    void* data, struct wl_callback* callback, uint32_t serial) {
  struct sl_window* window =
      static_cast<sl_window*>(wl_callback_get_user_data(callback));

  window->configure_event_barrier = nullptr;

  if (window->coalesced_next_config.serial) {
    window->next_config = window->coalesced_next_config;
    window->coalesced_next_config.serial = 0;
    sl_internal_xdg_surface_configure(data, window->xdg_surface,
                                      window->next_config.serial);
  }
}

////////////////////////////////////////////////////////////////////////////////
// common toplevel code

static const int32_t kUnspecifiedCoord = INT32_MIN;

void sl_window_update_should_be_containerized_from_pid(
    struct sl_window* window) {
  bool quirk_enabled = false;
#ifdef QUIRKS_SUPPORT
  quirk_enabled = window->ctx->quirks.IsEnabled(
      window, quirks::FEATURE_CONTAINERIZE_WINDOWS);
#endif
  if (!quirk_enabled || !window->pid) {
    return;
  }

  // If the binary name contains certain keywords, it is probably not actually a
  // game.
  char comm_file_path[50];
  snprintf(comm_file_path, sizeof(comm_file_path), "/proc/%d/comm",
           window->pid);
  std::ifstream proc_comm_file(comm_file_path);
  if (proc_comm_file.is_open()) {
    std::string process_name;
    if (getline(proc_comm_file, process_name)) {
      std::transform(process_name.begin(), process_name.end(),
                     process_name.begin(), tolower);
      window->should_be_containerized_from_pid =
          (process_name.find("launcher") == std::string::npos &&
           process_name.find("easyanticheat") == std::string::npos &&
           process_name.find("battleeye") == std::string::npos &&
           process_name.find("nprotect") == std::string::npos);
    }
    proc_comm_file.close();
  }

  // TODO(endlesspring): Consider other aspects of the process - memory
  // usage, library usage, etc to determine.
}

bool sl_window_is_containerized(struct sl_window* window) {
  bool window_containerized = false;
#ifdef QUIRKS_SUPPORT
  bool probably_game_window =
      // Steam game ID property is set
      window->steam_game_id &&
      // Window type is normal
      window->type ==
          window->ctx->atoms[ATOM_NET_WM_WINDOW_TYPE_NORMAL].value &&
      // Based on the PID and derived information, the window is probably a game
      // window.
      window->should_be_containerized_from_pid &&
      // Window max dimensions are either not set or if set, bigger (in
      // half-perimeter) than specified value.
      ((window->max_width + window->max_height == 0) ||
       (window->max_width + window->max_height >= 400));
  if (window_containerized && !probably_game_window) {
    LOG(VERBOSE) << window
                 << " is not a game window! max_width=" << window->max_width;
  }
  window_containerized =
      probably_game_window && window->ctx->quirks.IsEnabled(
                                  window, quirks::FEATURE_CONTAINERIZE_WINDOWS);
#endif
  return window_containerized;
}

void sl_window_reset_viewport(struct sl_window* window) {
  LOG(VERBOSE) << window << " viewport override unset";
  window->viewport_width = -1;
  window->viewport_height = -1;
  window->viewport_override = false;
  // Note that viewport destination is reset during surface_commit.
}

// Handle a configure event on a toplevel object from the compositor.
//
// window: The window being configured.
// x: The configured X coordinate in host logical space, or kUnspecifiedCoord
//    for toplevel objects that don't send positions.
// y: The configured Y coordinate in host logical space, or kUnspecifiedCoord
//    for toplevel objects that don't send positions.
// width, height: Configured size in host logical space.
// states: Array of XDG_TOPLEVEL_STATE_* enum values.
void sl_internal_toplevel_configure(struct sl_window* window,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    struct wl_array* states) {
  if (!window->managed ||
      (window->ctx->ignore_stateless_toplevel_configure && states->size == 0)) {
    // Ignore requests for non-managed windows.
    // Ignore requests with no states if --ignore-stateless-toplevel-configure
    // is set.
    return;
  }

  bool window_containerized = sl_window_is_containerized(window);

  if (window_containerized) {
    // Process window states first, so that window position and size can be
    // configured based on the states. If the window is not containerized, we
    // process the states last.
    sl_internal_toplevel_configure_state_containerized(window, states);
  }

  if (width && height) {
    int32_t width_in_pixels = width;
    int32_t height_in_pixels = height;

    // We are receiving a request to resize a window (in logical dimensions)
    // If the request is equal to the cached values we used to make adjustments
    // do not recalculate the values
    // However, if the request is not equal to the cached values, try
    // and keep the buffer same size as what was previously set
    // by the application.
    struct sl_host_surface* paired_surface = window->paired_surface;

    if (paired_surface && paired_surface->has_own_scale) {
      if (width != paired_surface->cached_logical_width ||
          height != paired_surface->cached_logical_height) {
        sl_transform_try_window_scale(window->ctx, paired_surface,
                                      window->width, window->height);
      }
    }
    sl_transform_host_to_guest(window->ctx, window->paired_surface,
                               &width_in_pixels, &height_in_pixels);

    int config_idx = 0;
    window->next_config.mask = 0;
    // Refer to enum xcb_config_window_t for the order of configuration,
    // position must be done before size.
    sl_internal_toplevel_configure_position(window, x, y, width_in_pixels,
                                            height_in_pixels, config_idx);

    // We don't need to resize windows using viewports that are windowed and
    // resizble.
    if (window_containerized) {
      sl_internal_toplevel_configure_size_containerized(
          window, x, y, width, height, width_in_pixels, height_in_pixels,
          config_idx);
    } else {
      sl_internal_toplevel_configure_size(window, x, y, width, height,
                                          width_in_pixels, height_in_pixels,
                                          config_idx);
    }
  }

  if (!window_containerized) {
    sl_internal_toplevel_configure_state(window, states);
  }
}

void sl_internal_toplevel_configure_size_containerized(struct sl_window* window,
                                                       int32_t x,
                                                       int32_t y,
                                                       int32_t host_width,
                                                       int32_t host_height,
                                                       int32_t width_in_pixels,
                                                       int32_t height_in_pixels,
                                                       int& mut_config_idx) {
  // Check the game is windowed (with decorations, indicating that this is not
  // borderless windowed), and has hinted that it can be resized to the size
  // that Exo requested. Note that max size 0 means no limits. Note that min&max
  // dimensions are strictly set by the X11 client only (and forwarded to Exo
  // immediately).
  bool windowed_and_resizable =
      (!window->fullscreen && window->decorated) &&
      (window->max_width >= width_in_pixels || !window->max_width) &&
      window->min_width <= width_in_pixels &&
      (window->max_height >= height_in_pixels || !window->max_height) &&
      window->min_height <= height_in_pixels;
  // Forward resizes to the client if requested size fits within the min&max
  // dimensions.
  if (windowed_and_resizable && !window->use_emulated_rects) {
    // TODO(b/330639760): Consider unset aspect ratio every frame in
    // surface_commit.
    zaura_surface_set_aspect_ratio(window->aura_surface, -1, -1);
    sl_window_reset_viewport(window);
    window->next_config.mask |= XCB_CONFIG_WINDOW_WIDTH |
                                XCB_CONFIG_WINDOW_HEIGHT |
                                XCB_CONFIG_WINDOW_BORDER_WIDTH;
    window->next_config.values[mut_config_idx++] = width_in_pixels;
    window->next_config.values[mut_config_idx++] = height_in_pixels;
    window->next_config.values[mut_config_idx++] = 0;
    LOG(VERBOSE) << window << " size set to " << width_in_pixels << "x"
                 << height_in_pixels;
    return;
  }

  auto output = window->paired_surface->output.get();

  // Set the window size to be either max or min window size, set by the X11
  // client. If width/height min and max are 0, set the window size to the size
  // of the screen. If the window is fullscreen (by the client), ignore size
  // hints and set it to the size of the screen.
  // We are effectively maximizing the window as much as we can within the
  // specified range, then down-scaling. This is to have consistent window
  // rendering experience with the most-expected use-case (fullscreen game,
  // user hits fullscreen toggle key). However, once we have better upscaling
  // algorithm, we should consider minimizing the size (or average?) then
  // upscaling in viewports instead.
  int safe_window_width =
      window->max_width ? window->max_width : window->min_width;
  int safe_window_height =
      window->max_height ? window->max_height : window->min_height;
  if (window->use_emulated_rects) {
    LOG(VERBOSE) << "using emulated rects " << window->emulated_width << "x"
                 << window->emulated_height;
    // If screen size emulation is set by XWayland, set the window size to the
    // emulated size and adjust the viewport size as Exo had requested it to be.
    safe_window_width = window->emulated_width;
    safe_window_height = window->emulated_height;
  } else if (!safe_window_width || !safe_window_height ||
             window->max_width > output->width ||
             window->max_height > output->height || window->fullscreen) {
    safe_window_width = output->width;
    safe_window_height = output->height;
  }

  window->next_config.mask |= XCB_CONFIG_WINDOW_WIDTH |
                              XCB_CONFIG_WINDOW_HEIGHT |
                              XCB_CONFIG_WINDOW_BORDER_WIDTH;
  window->next_config.values[mut_config_idx++] = safe_window_width;
  window->next_config.values[mut_config_idx++] = safe_window_height;
  window->next_config.values[mut_config_idx++] = 0;
  LOG(VERBOSE) << window << " size(safe) set to " << safe_window_width << "x"
               << safe_window_height;

  if (window->use_emulated_rects && window->compositor_fullscreen) {
    // Some games using emulated rects have xwayland send the wrong viewport
    // destination, causing the game to render in a small box. This overrides
    // that via quirk to set the destination to the size of the screen.
    if (sl_window_should_fix_randr_emu(window)) {
      window->viewport_override = true;
      window->viewport_width = host_width;
      window->viewport_height = host_height;
      window->viewport_pointer_scale =
          static_cast<float>(output->logical_width) / window->viewport_width;
      return;
    }
    // If we are using emulated rects and the window is fullscreen in
    // compositor, we only have to report the emulated size (done above). Aspect
    // ratio changes are moot (since compositor fullscreen). We don't have to
    // set viewport overrides since we can fully rely on viewport setup by
    // Xwayland.
    // If the window is windowed in compositor, then we have to do additional
    // viewport operations to make the window fit into the size that compositor
    // wants it to be, without disrupting the emulation. This has identical code
    // path as non-emulated mode, except pointer movement scaling (see below).
    sl_window_reset_viewport(window);
    return;
  }

  // Use viewport to resize surface as per Exo request if we are not using
  // emulated rects, or window is not fullscreen in Exo (and using emulated
  // rects).
  window->viewport_override = true;

  int32_t safe_window_width_in_wl = safe_window_width;
  int32_t safe_window_height_in_wl = safe_window_height;
  sl_transform_guest_to_host(window->ctx, window->paired_surface,
                             &safe_window_width_in_wl,
                             &safe_window_height_in_wl);
  // TODO(endlesspring): consider ignoring aspect ratio and filling the entire
  // screen if Exo wants the window to be fullscreen. This wills require having
  // pointer scale in x and y, instead of one.
  // if (window->compositor_fullscreen) {
  //   window->viewport_width = output->logical_width;
  //   window->viewport_height = output->logical_height;
  //   window->viewport_pointer_scale =
  //       static_cast<float>(safe_window_width_in_wl) / window->viewport_width;
  //   return;
  // }

  // TODO(b/330639760): Once aspect ratio is working correctly, we can entirely
  // get rid of this ratio calculation, in theory.
  // Adjust viewport while maintaining aspect ratio
  float width_ratio = static_cast<float>(safe_window_width) / width_in_pixels;
  float height_ratio =
      static_cast<float>(safe_window_height) / height_in_pixels;
  if (std::abs(width_ratio - height_ratio) < 0.005) {
    window->viewport_width = host_width;
    window->viewport_height = host_height;
    // If Exo sets a size that is of a different aspect ratio we shrink
    // the window to maintain aspect ratio. We do this by always shrinking
    // the side that is now proportionally larger as if we expand the
    // smaller side it might result in the window becoming larger than the
    // allowed bounds of the screen.
  } else if (width_ratio < height_ratio) {
    window->viewport_width =
        (static_cast<float>(safe_window_width) * host_height) /
        safe_window_height;
    window->viewport_height = host_height;

  } else if (width_ratio > height_ratio) {
    window->viewport_height =
        (static_cast<float>(safe_window_height) * host_width) /
        safe_window_width;
    window->viewport_width = host_width;
  }
  LOG(VERBOSE) << window << " viewport set to " << window->viewport_width << "x"
               << window->viewport_height;

  // TODO(b/330639760): For some reason, set_aspect_ratio includes the title
  // bar in the surface ratio. Figure out a way to mitigate this.
  // Also consider setting aspect ratio every frame in surface_commit.
  zaura_surface_set_aspect_ratio(
      window->aura_surface, window->viewport_width,
      window->viewport_height + (window->compositor_fullscreen ? 0 : 32));

  if (window->use_emulated_rects) {
    // Pointer scaling is being done in XWayland as well, assuming the viewport
    // is the size of the screen. Map the movement from viewport to logical
    // width of the screen (which XWayland is expecting).
    window->viewport_pointer_scale =
        static_cast<float>(output->logical_width) / window->viewport_width;
  } else {
    // Map movement from viewport to the window's space.
    window->viewport_pointer_scale =
        static_cast<float>(safe_window_width_in_wl) / window->viewport_width;
  }

  if (window->xdg_toplevel) {
    // Override X11 client-defined min and max size to a value relative to
    // display screen size.
    // When client client updates min/max, window->min/max_width/height values
    // are updated, then xdg_toplevel calls are made (see
    // sl_handle_property_notify for details). This results in Exo adjusting
    // the window size appropriately, resulting in this snippet
    // (part of toplevel_configure) to run and override the max/min again. This
    // allows the X11 client to continue resize itself but also allow the user
    // to resize the window at will within the constraints.
    xdg_toplevel_set_max_size(window->xdg_toplevel, output->logical_width * 0.8,
                              output->logical_height * 0.8);
    xdg_toplevel_set_min_size(window->xdg_toplevel, output->logical_width * 0.4,
                              output->logical_height * 0.4);
  }
}

void sl_internal_toplevel_configure_size(struct sl_window* window,
                                         int32_t x,
                                         int32_t y,
                                         int32_t host_width,
                                         int32_t host_height,
                                         int32_t width_in_pixels,
                                         int32_t height_in_pixels,
                                         int& mut_config_idx) {
  // For games with issues with incorrect viewport destination when using randr
  // emulation, this will override the viewport destination with the screen size
  // when in fullscreen mode.
  if (window->use_emulated_rects && window->compositor_fullscreen &&
      sl_window_should_fix_randr_emu(window)) {
    window->viewport_override = true;
    window->viewport_width = host_width;
    window->viewport_height = host_height;
    window->viewport_pointer_scale =
        static_cast<float>(window->width) / window->viewport_width;
    // Workaround using viewport for when Exo sets sizes that are not
    // within the bounds the client requested.
    // TODO(b/316990641): Implement resizing via viewports if
    // window->fullscreen==true but window->compositor_fullscreen==false.
    // Only do this when --only-client-can-exit-fullscreen is set.
  } else if (window->ctx->viewport_resize &&
             ((window->max_width != 0 && width_in_pixels > window->max_width) ||
              (window->min_width != 0 && width_in_pixels < window->min_width) ||
              (window->max_height != 0 &&
               height_in_pixels > window->max_height) ||
              (window->min_height != 0 &&
               height_in_pixels < window->min_height))) {
    window->viewport_override = true;
    float width_ratio = static_cast<float>(window->width) / width_in_pixels;
    float height_ratio = static_cast<float>(window->height) / height_in_pixels;
    if (std::abs(width_ratio - height_ratio) < 0.01) {
      window->viewport_override = true;
      window->viewport_width = host_width;
      window->viewport_height = host_height;
      // If Exo sets a size that is of a different aspect ratio we shrink
      // the window to maintain aspect ratio. We do this by always shrinking
      // the side that is now proportionally larger as if we expand the
      // smaller side it might result in the window becoming larger than the
      // allowed bounds of the screen.
    } else if (width_ratio < height_ratio) {
      window->viewport_width =
          (static_cast<float>(window->width) * host_height) / window->height;
      window->viewport_height = host_height;

    } else if (width_ratio > height_ratio) {
      window->viewport_height =
          (static_cast<float>(window->height) * host_width) / window->width;
      window->viewport_width = host_width;
    }

    int32_t window_width = window->width;
    int32_t window_height = window->height;
    sl_transform_guest_to_host(window->ctx, window->paired_surface,
                               &window_width, &window_height);
    window->viewport_pointer_scale =
        static_cast<float>(window_width) / window->viewport_width;
    xdg_toplevel_set_min_size(window->xdg_toplevel, window->viewport_width,
                              window->viewport_height);
    xdg_toplevel_set_max_size(window->xdg_toplevel, window->viewport_width,
                              window->viewport_height);
  } else if (window->viewport_override) {
    sl_window_reset_viewport(window);
    wp_viewport_shim()->set_destination(window->paired_surface->viewport,
                                        window->viewport_width,
                                        window->viewport_height);
  }

  window->next_config.mask |= XCB_CONFIG_WINDOW_WIDTH |
                              XCB_CONFIG_WINDOW_HEIGHT |
                              XCB_CONFIG_WINDOW_BORDER_WIDTH;
  if (window->viewport_override) {
    LOG(VERBOSE) << window << " viewport override, size set to "
                 << window->width << "x" << window->height;
    window->next_config.values[mut_config_idx++] = window->width;
    window->next_config.values[mut_config_idx++] = window->height;
    window->next_config.values[mut_config_idx++] = 0;
  } else if (window->use_emulated_rects) {
    LOG(VERBOSE) << window << " emulated, size set to "
                 << window->emulated_width << "x" << window->emulated_height;
    window->next_config.values[mut_config_idx++] = window->emulated_width;
    window->next_config.values[mut_config_idx++] = window->emulated_height;
    window->next_config.values[mut_config_idx++] = 0;
  } else {
    LOG(VERBOSE) << window << " size set to " << width_in_pixels << "x"
                 << height_in_pixels;
    window->next_config.values[mut_config_idx++] = width_in_pixels;
    window->next_config.values[mut_config_idx++] = height_in_pixels;
    window->next_config.values[mut_config_idx++] = 0;
  }
}

void sl_internal_toplevel_configure_position(struct sl_window* window,
                                             uint32_t x,
                                             uint32_t y,
                                             int32_t width_in_pixels,
                                             int32_t height_in_pixels,
                                             int& mut_config_idx) {
  if (window->use_emulated_rects) {
    // Ignore requests from the compositor and set the coordinates as emulation
    // requested.
    window->next_config.mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    window->next_config.values[mut_config_idx++] = window->emulated_x;
    window->next_config.values[mut_config_idx++] = window->emulated_y;
    LOG(VERBOSE) << window << " position set to emulated " << window->emulated_x
                 << "," << window->emulated_y;

  } else if (x != kUnspecifiedCoord && y != kUnspecifiedCoord) {
    // Convert to virtual coordinates
    int32_t guest_x = x;
    int32_t guest_y = y;

    sl_transform_host_position_to_guest_position(
        window->ctx, window->paired_surface, &guest_x, &guest_y);

    window->next_config.mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    window->next_config.values[mut_config_idx++] = guest_x;
    window->next_config.values[mut_config_idx++] = guest_y;
    LOG(VERBOSE) << window << " position set to specified " << guest_x << ","
                 << guest_y;

  } else if (!(window->size_flags & (US_POSITION | P_POSITION))) {
    uint32_t new_x = 0;
    uint32_t new_y = 0;

    const sl_host_output* output =
        window->paired_surface ? window->paired_surface->output.get() : nullptr;
    if (output) {
      if (sl_window_is_containerized(window) && window->viewport_override &&
          window->fullscreen && !window->compositor_fullscreen) {
        new_x = output->virt_x;
        new_y = output->virt_y;
      } else {
        new_x =
            output->virt_x + (output->virt_rotated_width - width_in_pixels) / 2;
        new_y = (output->virt_rotated_height - height_in_pixels) / 2;
      }
    } else {
      new_x = window->ctx->screen->width_in_pixels / 2 - width_in_pixels / 2;
      new_y = window->ctx->screen->height_in_pixels / 2 - height_in_pixels / 2;
    }

    window->next_config.mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    window->next_config.values[mut_config_idx++] = new_x;
    window->next_config.values[mut_config_idx++] = new_y;
    LOG(VERBOSE) << window << " position set to new " << new_x << "," << new_y;
  }
}

void sl_internal_toplevel_configure_set_activated(struct sl_window* window,
                                                  bool activated) {
  if (activated != window->activated) {
    if (activated != (window->ctx->host_focus_window == window)) {
      window->ctx->host_focus_window = activated ? window : nullptr;
      window->ctx->needs_set_input_focus = 1;
    }
    window->activated = activated;
  }
}

void sl_internal_toplevel_configure_state(struct sl_window* window,
                                          struct wl_array* states) {
  // States: https://wayland.app/protocols/xdg-shell#xdg_toplevel:enum:state
  // Note that if there are no states specified, it is not-maximized,
  // not-fullscreen, not-currently-being-resized and not-activated window.
  bool activated = false;
  window->allow_resize = 1;
  window->compositor_fullscreen = 0;

  uint32_t* state;
  int state_idx = 0;

  if (window->ctx->only_client_can_exit_fullscreen && window->fullscreen) {
    // If the window has been set to full screen by the client, do not try to
    // revert it back. Many games and applications fail to adapt to changes
    // enforced by the WM (ie. the fullscreen state must be toggled via the
    // game's UI).
    window->next_config.states[state_idx++] =
        window->ctx->atoms[ATOM_NET_WM_STATE_FULLSCREEN].value;
    window->allow_resize = 0;
  }

  sl_array_for_each(state, states) {
    if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) {
      if (state_idx == 0) {
        // Only add fullscreen state if it was previously not added by
        // --only-client-can-exit-fullscreen
        window->next_config.states[state_idx++] =
            window->ctx->atoms[ATOM_NET_WM_STATE_FULLSCREEN].value;
      }
      window->allow_resize = 0;
      window->compositor_fullscreen = 1;
    }
    if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED) {
      bool force_x11_unmaximize = false;
#ifdef QUIRKS_SUPPORT
      force_x11_unmaximize = window->ctx->quirks.IsEnabled(
          window, quirks::FEATURE_FORCE_X11_UNMAXIMIZE);
#endif
      window->allow_resize = 0;
      if (!force_x11_unmaximize) {
        window->next_config.states[state_idx++] =
            window->ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT].value;
        window->next_config.states[state_idx++] =
            window->ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ].value;
      }
    }
    if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
      activated = true;
      window->next_config.states[state_idx++] =
          window->ctx->atoms[ATOM_NET_WM_STATE_FOCUSED].value;
    }
    if (*state == XDG_TOPLEVEL_STATE_RESIZING)
      window->allow_resize = 0;
  }

  sl_internal_toplevel_configure_set_activated(window, activated);

  LOG(VERBOSE) << window << " state change, fullscreen=" << window->fullscreen
               << " compositor_fullscreen=" << window->compositor_fullscreen;
  // Override previous state definitions
  window->next_config.states_length = state_idx;
}

void sl_internal_toplevel_configure_state_containerized(
    struct sl_window* window, struct wl_array* states) {
  // States: https://wayland.app/protocols/xdg-shell#xdg_toplevel:enum:state
  // Note that if there are no states specified, it is not-maximized,
  // not-fullscreen, not-currently-being-resized and not-activated window.
  bool activated = false;
  window->allow_resize = 1;
  window->compositor_fullscreen = 0;

  uint32_t* state;
  int state_idx = 0;

  // Keep the fullscreen state if X11 client decided to be fullscreen. Ignore
  // compositor's request.
  if (window->fullscreen) {
    window->allow_resize = 0;
    window->next_config.states[state_idx++] =
        window->ctx->atoms[ATOM_NET_WM_STATE_FULLSCREEN].value;
  }

  // Ditto as above, but maximize state.
  if (window->maximized) {
    bool force_x11_unmaximize = false;
#ifdef QUIRKS_SUPPORT
    force_x11_unmaximize = window->ctx->quirks.IsEnabled(
        window, quirks::FEATURE_FORCE_X11_UNMAXIMIZE);
#endif
    window->allow_resize = 0;
    if (!force_x11_unmaximize) {
      window->next_config.states[state_idx++] =
          window->ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT].value;
      window->next_config.states[state_idx++] =
          window->ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ].value;
    }
  }

  sl_array_for_each(state, states) {
    // Note that we are ignoring maximized state for reasons specified above. We
    // are not even taking note of what the compositor wants since we don't have
    // to for window containerization.
    if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) {
      // Mark as compositor request to be fullscreen, so that other sizing
      // operations can operate to fulfill Exo's request.
      window->compositor_fullscreen = 1;
    }
    if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
      activated = true;
      window->next_config.states[state_idx++] =
          window->ctx->atoms[ATOM_NET_WM_STATE_FOCUSED].value;
    }
    if (*state == XDG_TOPLEVEL_STATE_RESIZING) {
      window->allow_resize = 0;
    }
  }

  // Set focus appropriately.
  sl_internal_toplevel_configure_set_activated(window, activated);

  if (!window->compositor_fullscreen) {
    // Ignore the window decoration settings done by the X11 client, and show
    // frame decorations if Exo treats the surface as non-fullscreen.
    zaura_surface_set_frame(window->aura_surface,
                            ZAURA_SURFACE_FRAME_TYPE_NORMAL);
  }

  LOG(VERBOSE) << window << " state change, fullscreen=" << window->fullscreen
               << " compositor_fullscreen=" << window->compositor_fullscreen;
  // Override previous state definitions
  window->next_config.states_length = state_idx;
}

////////////////////////////////////////////////////////////////////////////////
// xdg_toplevel event listeners
//
// https://crsrc.org/s/?q=f:sommelier/protocol/xdg-shell.xml%20name=\"xdg_toplevel
//
// In Exo, this is sent from
// https://crsrc.org/s/?q=f:exo%2Fwayland.*cc%20xdg_toplevel_send_configure
static void sl_internal_xdg_toplevel_configure(
    void* unused_data,
    struct xdg_toplevel* xdg_toplevel,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  TRACE_EVENT("other", "sl_internal_xdg_toplevel_configure");
  struct sl_window* window =
      static_cast<sl_window*>(xdg_toplevel_get_user_data(xdg_toplevel));
  sl_internal_toplevel_configure(window, kUnspecifiedCoord, kUnspecifiedCoord,
                                 width, height, states);
}

// In Exo, this is sent from
// https://crsrc.org/s/?q=f:exo%2Fwayland.*cc%20xdg_toplevel_send_close
static void sl_internal_xdg_toplevel_close(void* data,
                                           struct xdg_toplevel* xdg_toplevel) {
  TRACE_EVENT("other", "sl_internal_xdg_toplevel_close");
  struct sl_window* window =
      static_cast<sl_window*>(xdg_toplevel_get_user_data(xdg_toplevel));
  xcb_client_message_event_t event = {};
  event.response_type = XCB_CLIENT_MESSAGE;
  event.format = 32;
  event.window = window->id;
  event.type = window->ctx->atoms[ATOM_WM_PROTOCOLS].value;
  event.data.data32[0] = window->ctx->atoms[ATOM_WM_DELETE_WINDOW].value;
  event.data.data32[1] = XCB_CURRENT_TIME;

  xcb_send_event(window->ctx->connection, 0, window->id,
                 XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
}

static const struct xdg_toplevel_listener sl_internal_xdg_toplevel_listener = {
    sl_internal_xdg_toplevel_configure, sl_internal_xdg_toplevel_close};

////////////////////////////////////////////////////////////////////////////////
// zaura_toplevel event listeners
//
// https://crsrc.org/s/?q=f:sommelier/protocol/aura-shell.xml%20name=\"zaura_toplevel

// Sent from Exo here:
// https://crsrc.org/s/?q=f:exo%2Fwayland.*cc%20zaura_toplevel_send_configure
static void sl_internal_zaura_toplevel_configure(
    void* unused_data,
    struct zaura_toplevel* zaura_toplevel,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  TRACE_EVENT("other", "sl_internal_zaura_toplevel_configure");
  struct sl_window* window =
      static_cast<sl_window*>(zaura_toplevel_get_user_data(zaura_toplevel));

  // aura_toplevel.configure replaces xdg_toplevel.configure for surfaces on
  // which zaura_toplevel_set_supports_screen_coordinates() has been called.
  // So we shouldn't get duplicate events.
  //
  // TODO(cpelling): Handle aura-specific states.
  sl_internal_toplevel_configure(window, x, y, width, height, states);
}

// Sent from Exo here:
// https://crsrc.org/s/?q=f:exo%2Fwayland.*cc%20zaura_toplevel_send_origin_change
static void sl_internal_zaura_toplevel_origin_change(
    void* data, struct zaura_toplevel* zaura_toplevel, int32_t x, int32_t y) {
  // aura_toplevel.origin_change is not part of the normal configuration
  // lifecycle, and is not followed by xdg_surface.configure. So just apply
  // this change immediately.
  sl_window* window =
      static_cast<sl_window*>(zaura_toplevel_get_user_data(zaura_toplevel));

  if (window->configure_event_barrier) {
    // TODO(cpelling): Coalesce origin_change events instead of dropping them.
    return;
  }

  int32_t guest_x = x;
  int32_t guest_y = y;
  sl_transform_host_position_to_guest_position(
      window->ctx, window->paired_surface, &guest_x, &guest_y);
  window->x = guest_x;
  window->y = guest_y;

  uint32_t values[2];
  sl_window_get_x_y(window, &values[0], &values[1]);
  xcb()->configure_window(window->ctx->connection, window->frame_id,
                          XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
}

static const struct zaura_toplevel_listener
    sl_internal_zaura_toplevel_listener = {
        sl_internal_zaura_toplevel_configure,
        sl_internal_zaura_toplevel_origin_change};

static const struct wl_callback_listener configure_event_barrier_listener = {
    sl_internal_xdg_surface_configure_barrier_done};

void sl_toplevel_send_window_bounds_to_host(struct sl_window* window) {
  // Don't send window bounds if fullscreen/maximized/resizing,
  // or if the feature is unsupported by the host or disabled by flag.
  if (!window->allow_resize || !sl_window_is_client_positioned(window) ||
      !window->ctx->aura_shell ||
      window->ctx->aura_shell->version <
          ZAURA_TOPLEVEL_SET_WINDOW_BOUNDS_SINCE_VERSION ||
      !window->aura_toplevel) {
    return;
  }
  int32_t x = window->x;
  int32_t y = window->y;
  int32_t w = window->width;
  int32_t h = window->height;
  if (window->size_flags & P_MIN_SIZE) {
    if (w < window->min_width)
      w = window->min_width;
    if (h < window->min_height)
      h = window->min_height;
  }
  if (window->size_flags & P_MAX_SIZE) {
    if (w > window->max_width)
      w = window->max_width;
    if (h > window->max_height)
      h = window->max_height;
  }

  sl_host_output* output = sl_transform_guest_position_to_host_position(
      window->ctx, window->paired_surface, &x, &y);
  sl_transform_guest_to_host(window->ctx, window->paired_surface, &w, &h);

  zaura_toplevel_set_window_bounds(window->aura_toplevel, x, y, w, h,
                                   output->proxy);

  if (window->configure_event_barrier) {
    wl_callback_destroy(window->configure_event_barrier);
  }
  window->configure_event_barrier = wl_display_sync(window->ctx->display);
  wl_callback_add_listener(window->configure_event_barrier,
                           &configure_event_barrier_listener, window);
}

////////////////////////////////////////////////////////////////////////////////

void sl_update_application_id(struct sl_context* ctx,
                              struct sl_window* window) {
  TRACE_EVENT("other", "sl_update_application_id");
  if (!window->aura_surface)
    return;
  if (ctx->application_id) {
    zaura_surface_set_application_id(window->aura_surface, ctx->application_id);
    return;
  }
  // Don't set application id for X11 override redirect. This prevents
  // aura shell from thinking that these are regular application windows
  // that should appear in application lists.
  if (!ctx->xwayland || window->managed) {
    char* application_id_str;
    if (!window->app_id_property.empty()) {
      application_id_str =
          sl_xasprintf(X11_PROPERTY_APPLICATION_ID_FORMAT, ctx->vm_id,
                       window->app_id_property.c_str());
    } else if (window->clazz) {
      application_id_str = sl_xasprintf(WM_CLASS_APPLICATION_ID_FORMAT,
                                        ctx->vm_id, window->clazz);
    } else if (window->client_leader != XCB_WINDOW_NONE) {
      application_id_str = sl_xasprintf(WM_CLIENT_LEADER_APPLICATION_ID_FORMAT,
                                        ctx->vm_id, window->client_leader);
    } else {
      application_id_str =
          sl_xasprintf(XID_APPLICATION_ID_FORMAT, ctx->vm_id, window->id);
    }

    zaura_surface_set_application_id(window->aura_surface, application_id_str);
    free(application_id_str);
  }
}

void sl_window_update(struct sl_window* window) {
  TRACE_EVENT("surface", "sl_window_update", "id", window->id);
  struct wl_resource* host_resource = nullptr;
  struct sl_host_surface* host_surface;
  struct sl_context* ctx = window->ctx;
  struct sl_window* parent = nullptr;

  if (window->host_surface_id) {
    host_resource = wl_client_get_object(ctx->client, window->host_surface_id);
    if (host_resource && window->unpaired) {
      wl_list_remove(&window->link);
      wl_list_insert(&ctx->windows, &window->link);
      window->unpaired = 0;
    }
  } else if (!window->unpaired) {
    wl_list_remove(&window->link);
    wl_list_insert(&ctx->unpaired_windows, &window->link);
    window->unpaired = 1;
    window->paired_surface->window = nullptr;
    window->paired_surface = nullptr;
  }

  if (!host_resource) {
    if (window->aura_surface) {
      zaura_surface_destroy(window->aura_surface);
      window->aura_surface = nullptr;
    }
    if (window->xdg_toplevel) {
      xdg_toplevel_destroy(window->xdg_toplevel);
      window->xdg_toplevel = nullptr;
    }
    if (window->xdg_popup) {
      xdg_popup_destroy(window->xdg_popup);
      window->xdg_popup = nullptr;
    }
    if (window->xdg_surface) {
      xdg_surface_destroy(window->xdg_surface);
      window->xdg_surface = nullptr;
    }
    window->realized = 0;
    return;
  }

  host_surface =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(host_resource));
  assert(host_surface);
  assert(!host_surface->has_role);

  if (!window->unpaired) {
    window->paired_surface = host_surface;
    host_surface->window = window;
    sl_transform_try_window_scale(ctx, host_surface, window->width,
                                  window->height);
  }

  assert(ctx->xdg_shell);
  assert(ctx->xdg_shell->internal);

  if (window->managed) {
    if (window->transient_for != XCB_WINDOW_NONE) {
      struct sl_window* sibling;

      wl_list_for_each(sibling, &ctx->windows, link) {
        if (sibling->id == window->transient_for) {
          if (sibling->xdg_toplevel)
            parent = sibling;
          break;
        }
      }
    }
  }

  // If we have a transient parent, but could not find it in the list of
  // realized windows, then pick the window that had the last event for the
  // parent.  We update this again when we gain focus, so if we picked the wrong
  // one it can get corrected at that point (but it's also possible the parent
  // will never be realized, which is why selecting one here is important).
  if (!window->managed ||
      (!parent && window->transient_for != XCB_WINDOW_NONE)) {
    struct sl_window* sibling;
    uint32_t parent_last_event_serial = 0;

    wl_list_for_each(sibling, &ctx->windows, link) {
      struct wl_resource* sibling_host_resource;
      struct sl_host_surface* sibling_host_surface;

      if (!sibling->realized)
        continue;

      sibling_host_resource =
          wl_client_get_object(ctx->client, sibling->host_surface_id);
      if (!sibling_host_resource)
        continue;

      // Any parent will do but prefer last event window.
      sibling_host_surface = static_cast<sl_host_surface*>(
          wl_resource_get_user_data(sibling_host_resource));
      if (parent_last_event_serial > sibling_host_surface->last_event_serial)
        continue;

      // Do not use ourselves as the parent.
      if (sibling->host_surface_id == window->host_surface_id)
        continue;

      parent = sibling;
      parent_last_event_serial = sibling_host_surface->last_event_serial;
    }
  }

  if (!window->depth) {
    xcb_get_geometry_reply_t* geometry_reply = xcb()->get_geometry_reply(
        ctx->connection, xcb()->get_geometry(ctx->connection, window->id),
        nullptr);
    if (geometry_reply) {
      window->depth = geometry_reply->depth;
      free(geometry_reply);
    }
  }

  if (!window->xdg_surface) {
    window->xdg_surface = xdg_wm_base_get_xdg_surface(ctx->xdg_shell->internal,
                                                      host_surface->proxy);
    xdg_surface_add_listener(window->xdg_surface,
                             &sl_internal_xdg_surface_listener, window);
  }

  if (ctx->aura_shell) {
    uint32_t frame_color;

    if (!window->aura_surface) {
      window->aura_surface = zaura_shell_get_aura_surface(
          ctx->aura_shell->internal, host_surface->proxy);
    }

    zaura_surface_set_frame(window->aura_surface,
                            window->decorated ? ZAURA_SURFACE_FRAME_TYPE_NORMAL
                            : window->depth == 32
                                ? ZAURA_SURFACE_FRAME_TYPE_NONE
                                : ZAURA_SURFACE_FRAME_TYPE_SHADOW);

    frame_color = window->dark_frame ? ctx->dark_frame_color : ctx->frame_color;
    zaura_surface_set_frame_colors(window->aura_surface, frame_color,
                                   frame_color);
    zaura_surface_set_startup_id(window->aura_surface, window->startup_id);
    sl_update_application_id(ctx, window);

    if (ctx->aura_shell->version >=
        ZAURA_SURFACE_SET_FULLSCREEN_MODE_SINCE_VERSION) {
      zaura_surface_set_fullscreen_mode(window->aura_surface,
                                        ctx->fullscreen_mode);
    }
  }

  // Always use top-level surface for X11 windows as we can't control when the
  // window is closed.
  if (ctx->xwayland || !parent) {
    if (!window->xdg_toplevel) {
      window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
      xdg_toplevel_add_listener(window->xdg_toplevel,
                                &sl_internal_xdg_toplevel_listener, window);
    }

    // Right now, aura_toplevel is only needed for windows positioned by the
    // client (which is all windows if --enable-x11-move-windows is passed).
    // Setting it up means we get x and y coordinates in configure
    // events (aura_toplevel.configure replaces xdg_toplevel.configure), which
    // changes how windows are positioned in the X server's coordinate space. If
    // windows end up partially offscreen in that space, we get bugs like
    // b/269053427.
    //
    // Bottom line: If --enable-x11-move-windows is enabled, apps are
    // responsible for keeping themselves onscreen within X space. If not,
    // Sommelier is; in which case it should ignore the host compositor's
    // positioning decisions, since those are made without reference to X space.
    // Sommelier could listen to aura_toplevel.configure and ignore the x and y
    // coordinates, but for now the most conservative approach is to avoid using
    // aura_toplevel entirely. This can be revisited later if we need
    // aura_toplevel for anything else.
    if (sl_window_is_client_positioned(window) && ctx->aura_shell &&
        window->xdg_toplevel && !window->aura_toplevel) {
      window->aura_toplevel = zaura_shell_get_aura_toplevel_for_xdg_toplevel(
          ctx->aura_shell->internal, window->xdg_toplevel);
      zaura_toplevel_set_supports_screen_coordinates(window->aura_toplevel);
      zaura_toplevel_add_listener(window->aura_toplevel,
                                  &sl_internal_zaura_toplevel_listener, window);
    }

    if (parent)
      xdg_toplevel_set_parent(window->xdg_toplevel, parent->xdg_toplevel);
    if (window->name)
      xdg_toplevel_set_title(window->xdg_toplevel, window->name);
    if (window->size_flags & P_MIN_SIZE) {
      int32_t minw = window->min_width;
      int32_t minh = window->min_height;

      sl_transform_guest_to_host(window->ctx, window->paired_surface, &minw,
                                 &minh);
      xdg_toplevel_set_min_size(window->xdg_toplevel, minw, minh);
    }
    if (window->size_flags & P_MAX_SIZE) {
      int32_t maxw = window->max_width;
      int32_t maxh = window->max_height;

      sl_transform_guest_to_host(window->ctx, window->paired_surface, &maxw,
                                 &maxh);
      xdg_toplevel_set_max_size(window->xdg_toplevel, maxw, maxh);
    }
    if (window->maximized) {
      xdg_toplevel_set_maximized(window->xdg_toplevel);
    }
    if (window->fullscreen) {
      xdg_toplevel_set_fullscreen(window->xdg_toplevel, nullptr);
    }
  } else if (!window->xdg_popup) {
    struct xdg_positioner* positioner;
    int32_t diffx = window->x - parent->x;
    int32_t diffy = window->y - parent->y;

    positioner = xdg_wm_base_create_positioner(ctx->xdg_shell->internal);
    assert(positioner);

    sl_transform_guest_to_host(window->ctx, window->paired_surface, &diffx,
                               &diffy);
    xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
    xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    xdg_positioner_set_anchor_rect(positioner, diffx, diffy, 1, 1);

    window->xdg_popup = xdg_surface_get_popup(window->xdg_surface,
                                              parent->xdg_surface, positioner);
    xdg_popup_add_listener(window->xdg_popup, &sl_internal_xdg_popup_listener,
                           window);

    xdg_positioner_destroy(positioner);
  }

  if ((window->size_flags & (US_POSITION | P_POSITION)) && parent &&
      ctx->aura_shell) {
    int32_t diffx = window->x - parent->x;
    int32_t diffy = window->y - parent->y;

    sl_transform_guest_to_host(window->ctx, window->paired_surface, &diffx,
                               &diffy);
    zaura_surface_set_parent(window->aura_surface, parent->aura_surface, diffx,
                             diffy);
  }

#ifdef COMMIT_LOOP_FIX
  sl_commit(window, host_surface);
#else
  wl_surface_commit(host_surface->proxy);
#endif

  if (host_surface->contents_width && host_surface->contents_height)
    window->realized = 1;
}

#ifdef QUIRKS_SUPPORT
bool sl_window_should_log_quirk(struct sl_window* window, int feature_enum) {
  return window->logged_quirks.insert(feature_enum).second;
}

std::set<int> sl_window_logged_quirks(struct sl_window* window) {
  return window->logged_quirks;
}
#endif

bool sl_window_is_client_positioned(struct sl_window* window) {
#ifdef QUIRKS_SUPPORT
  if (window->ctx->quirks.IsEnabled(window, quirks::FEATURE_X11_MOVE_WINDOWS)) {
    return true;
  }
#endif
  return window->ctx->enable_x11_move_windows;
}

bool sl_window_should_fix_randr_emu(struct sl_window* window) {
#ifdef QUIRKS_SUPPORT
  if (window->ctx->quirks.IsEnabled(window, quirks::FEATURE_RANDR_EMU_FIX)) {
    return true;
  }
#endif
  return false;
}

void sl_window_get_x_y(struct sl_window* window, uint32_t* x, uint32_t* y) {
  if (x != nullptr) {
    *x = window->x;
  }
  if (y != nullptr) {
    *y = window->y;
  }
}

void sl_window_get_width_height(struct sl_window* window,
                                uint32_t* w,
                                uint32_t* h) {
  if (window->use_emulated_rects) {
    if (w != nullptr) {
      *w = window->emulated_width;
    }
    if (h != nullptr) {
      *h = window->emulated_height;
    }
  } else {
    if (w != nullptr) {
      *w = window->width;
    }
    if (h != nullptr) {
      *h = window->height;
    }
  }
}

bool sl_window_get_output_virt_position(struct sl_window* window,
                                        uint32_t& mut_x,
                                        uint32_t& mut_y) {
  for (auto output : window->ctx->host_outputs) {
    if (window->x >= output->virt_x &&
        window->x < output->virt_x + output->virt_width &&
        window->y >= output->virt_y &&
        window->y < output->virt_y + output->virt_height) {
      mut_x = output->virt_x;
      mut_y = output->virt_y;
      return true;
    }
  }
  return false;
}
