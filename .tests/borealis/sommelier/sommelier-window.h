// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_WINDOW_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_WINDOW_H_

#include <pixman.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <cstdint>
#include <set>
#include <string>
#include <xcb/xcb.h>

#define US_POSITION (1L << 0)
#define US_SIZE (1L << 1)
#define P_POSITION (1L << 2)
#define P_SIZE (1L << 3)
#define P_MIN_SIZE (1L << 4)
#define P_MAX_SIZE (1L << 5)
#define P_RESIZE_INC (1L << 6)
#define P_ASPECT (1L << 7)
#define P_BASE_SIZE (1L << 8)
#define P_WIN_GRAVITY (1L << 9)

struct sl_config {
  uint32_t serial = 0;
  uint32_t mask = 0;
  // Values and states here are consumed during sl_configure_window
  uint32_t values[5];
  uint32_t states_length = 0;
  uint32_t states[3];
};

struct sl_host_surface;

// An X11 window.
struct sl_window {
  sl_window(struct sl_context* ctx,
            xcb_window_t id,
            int x,
            int y,
            int width,
            int height,
            int border_width);
  ~sl_window();

  struct sl_context* ctx = nullptr;
  xcb_window_t id = XCB_WINDOW_NONE;
  xcb_window_t frame_id = XCB_WINDOW_NONE;
  uint32_t host_surface_id = 0;
  int unpaired = 1;
  bool shaped = false;

  // Window position and size are specified in X11's coordinate space
  // (Virtual Coordinate Space, as defined in sommelier-transform.h).
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  // Emulated x,y,width,height are 'faked' position and size by XWayland. We
  // only need to set and return this value to the clients (X11 windows). We
  // should continue doing normal window position and sizing operations with Exo
  // with real x,y,width,height.
  bool use_emulated_rects = false;
  int emulated_width = 0;
  int emulated_height = 0;
  int emulated_x = 0;
  int emulated_y = 0;

  int border_width = 0;
  int depth = 0;
  int managed = 0;
  int realized = 0;
  int activated = 0;
  int fullscreen = 0;
  int compositor_fullscreen = 0;
  int maximized = 0;
  int iconified = 0;
  // True if there has been changes to the fullscreen/maximized state
  // while this window is iconified.
  bool pending_fullscreen_change = false;
  bool pending_maximized_change = false;
  int allow_resize = 1;
  xcb_window_t transient_for = XCB_WINDOW_NONE;
  xcb_window_t client_leader = XCB_WINDOW_NONE;
  int decorated = 0;
  char* name = nullptr;
  bool has_net_wm_name = false;
  char* clazz = nullptr;
  char* startup_id = nullptr;
  uint32_t steam_game_id = 0;
  std::string app_id_property;
  int dark_frame = 0;
  uint32_t size_flags = P_POSITION;
  int focus_model_take_focus = 0;
  // TODO(b/331112549): introduce and utilize size structs for width/height
  int min_width = 0;
  int min_height = 0;
  int max_width = 0;
  int max_height = 0;

  // If true, use overridden viewport_width/height values to set destination.
  int viewport_override = false;
  // Determines scale factor for pointer movements. Incorrect value (i.e.
  // viewport scaling factor was not calculated correctly for X and Y axis.
  // Currently we assume X and Y axis have the same factor) will result in
  // pointer offset.
  double viewport_pointer_scale = 0;
  // Configured viewport destination width and height, usually done during
  // xdg_toplevel_configure. Only utilized if viewport_override is true.
  int viewport_width = -1;
  int viewport_height = -1;
  // The latest width and height that viewport.set_destination was called with
  // (ie. sent to Exo), usually done in host_surface_commit.
  int viewport_width_realized = -1;
  int viewport_height_realized = -1;

  // If true, check if borderless window should be set to fullscreen in Exo
  // during surface_commit. Initially set to true because some games (that
  // launch in borderless-windowed mode) assume the default state is fullscreen
  // not set.
  bool maybe_promote_to_fullscreen = true;

  // Value of _NET_WM_WINDOW_TYPE for the window set in x11.
  uint32_t type = 0;

  // PID of the window.
  uint32_t pid = 0;
  // If true, this window is probably a real game window based on derived
  // information from PID.
  bool should_be_containerized_from_pid = true;

#ifdef QUIRKS_SUPPORT
  // Quirk feature flags previously applied to this window, for which log
  // messages have already been written.
  std::set<int> logged_quirks;
#endif

  // Window rect and state from the most recent xdg_toplevel/aura_toplevel
  // configure event, to be applied when xdg_surface.configure is next received.
  struct sl_config next_config;

  // Window rect and state applied by xdg_surface.configure. Sommelier now waits
  // for the client to commit surface contents consistent with this config.
  struct sl_config pending_config;

  // When null, xdg_surface.configure events are processed immediately.
  // When set, all xdg_surface.configure events are coalesced together,
  // and Sommelier won't apply them until this callback's done event fires.
  // When the done event fires, the last received xdg_surface.configure event
  // is processed.
  struct wl_callback* configure_event_barrier = nullptr;

  // Most recent config received while the |configure_event_barrier| was active.
  struct sl_config coalesced_next_config;

  struct xdg_surface* xdg_surface = nullptr;
  struct xdg_toplevel* xdg_toplevel = nullptr;
  struct xdg_popup* xdg_popup = nullptr;
  struct zaura_surface* aura_surface = nullptr;
  struct zaura_toplevel* aura_toplevel = nullptr;
  struct sl_host_surface* paired_surface = nullptr;
  struct pixman_region32 shape_rectangles;
  struct wl_list link = {};
};

enum {
  PROPERTY_WM_NAME,
  PROPERTY_NET_WM_NAME,
  PROPERTY_WM_CLASS,
  PROPERTY_WM_TRANSIENT_FOR,
  PROPERTY_WM_NORMAL_HINTS,
  PROPERTY_WM_CLIENT_LEADER,
  PROPERTY_WM_PROTOCOLS,
  PROPERTY_MOTIF_WM_HINTS,
  PROPERTY_NET_STARTUP_ID,
  PROPERTY_NET_WM_PID,
  PROPERTY_NET_WM_STATE,
  PROPERTY_NET_WM_WINDOW_TYPE,
  PROPERTY_GTK_THEME_VARIANT,
  PROPERTY_XWAYLAND_RANDR_EMU_MONITOR_RECTS,
  PROPERTY_STEAM_GAME,

  // The atom corresponding to this property changes depending on the
  // --application-id-format command-line argument.
  PROPERTY_SPECIFIED_FOR_APP_ID,
};

struct sl_wm_size_hints {
  uint32_t flags;
  int32_t x, y;
  int32_t width, height;
  int32_t min_width, min_height;
  int32_t max_width, max_height;
  int32_t width_inc, height_inc;
  struct {
    int32_t x;
    int32_t y;
  } min_aspect, max_aspect;
  int32_t base_width, base_height;
  int32_t win_gravity;
};

// WM_HINTS is defined at: https://tronche.com/gui/x/icccm/sec-4.html

#define WM_HINTS_FLAG_INPUT (1L << 0)
#define WM_HINTS_FLAG_STATE (1L << 1)
#define WM_HINTS_FLAG_ICON_PIXMAP (1L << 2)
#define WM_HINTS_FLAG_ICON_WINDOW (1L << 3)
#define WM_HINTS_FLAG_ICON_POSITION (1L << 4)
#define WM_HINTS_FLAG_ICON_MASK (1L << 5)
#define WM_HINTS_FLAG_WINDOW_GROUP (1L << 6)
#define WM_HINTS_FLAG_MESSAGE (1L << 7)
#define WM_HINTS_FLAG_URGENCY (1L << 8)

struct sl_wm_hints {
  uint32_t flags;
  uint32_t input;
  uint32_t initial_state;
  xcb_pixmap_t icon_pixmap;
  xcb_window_t icon_window;
  int32_t icon_x;
  int32_t icon_y;
  xcb_pixmap_t icon_mask;
};

#define MWM_HINTS_FUNCTIONS (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_INPUT_MODE (1L << 2)
#define MWM_HINTS_STATUS (1L << 3)

#define MWM_DECOR_ALL (1L << 0)
#define MWM_DECOR_BORDER (1L << 1)
#define MWM_DECOR_RESIZEH (1L << 2)
#define MWM_DECOR_TITLE (1L << 3)
#define MWM_DECOR_MENU (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

struct sl_mwm_hints {
  uint32_t flags;
  uint32_t functions;
  uint32_t decorations;
  int32_t input_mode;
  uint32_t status;
};

#define NET_WM_MOVERESIZE_SIZE_TOPLEFT 0
#define NET_WM_MOVERESIZE_SIZE_TOP 1
#define NET_WM_MOVERESIZE_SIZE_TOPRIGHT 2
#define NET_WM_MOVERESIZE_SIZE_RIGHT 3
#define NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define NET_WM_MOVERESIZE_SIZE_BOTTOM 5
#define NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT 6
#define NET_WM_MOVERESIZE_SIZE_LEFT 7
#define NET_WM_MOVERESIZE_MOVE 8

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD 1
#define NET_WM_STATE_TOGGLE 2

#define WM_STATE_WITHDRAWN 0
#define WM_STATE_NORMAL 1
#define WM_STATE_ICONIC 3

void sl_window_update(struct sl_window* window);
void sl_toplevel_send_window_bounds_to_host(struct sl_window* window);
void sl_update_application_id(struct sl_context* ctx, struct sl_window* window);
void sl_configure_window(struct sl_window* window);
void sl_send_configure_notify(struct sl_window* window);
// TODO(b/331132756): Refactor containerization logic into more OO style, this
// will probably impact other areas of code as well.
void sl_internal_toplevel_configure(struct sl_window* window,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    struct wl_array* states);
void sl_internal_toplevel_configure_size(struct sl_window* window,
                                         int32_t x,
                                         int32_t y,
                                         int32_t host_width,
                                         int32_t host_height,
                                         int32_t width_in_pixels,
                                         int32_t height_in_pixels,
                                         int& mut_config_idx);
void sl_internal_toplevel_configure_size_containerized(struct sl_window* window,
                                                       int32_t x,
                                                       int32_t y,
                                                       int32_t host_width,
                                                       int32_t host_height,
                                                       int32_t width_in_pixels,
                                                       int32_t height_in_pixels,
                                                       int& mut_config_idx);
void sl_internal_toplevel_configure_position(struct sl_window* window,
                                             uint32_t x,
                                             uint32_t y,
                                             int32_t width_in_pixels,
                                             int32_t height_in_pixels,
                                             int& mut_config_idx);
void sl_internal_toplevel_configure_state(struct sl_window* window,
                                          struct wl_array* states);
void sl_internal_toplevel_configure_state_containerized(
    struct sl_window* window, struct wl_array* states);

int sl_process_pending_configure_acks(struct sl_window* window,
                                      struct sl_host_surface* host_surface);

#ifdef QUIRKS_SUPPORT
// Returns true if this function hasn't been called with this combination of
// `window` and `feature_enum` before. In that case, the caller is expected to
// write a log message indicating that the quirk has been applied.
bool sl_window_should_log_quirk(struct sl_window* window, int feature_enum);

// Returns all quirks ever logged against this window.
// This "latches"; if a quirk has ever been enabled, it will stay in this list,
// even if the quirk is no longer enabled.
std::set<int> sl_window_logged_quirks(struct sl_window* window);
#endif  // QUIRKS_SUPPORT

bool sl_window_is_client_positioned(struct sl_window* window);

// Set correct viewport destination from xwayland for incorrectly behaving
// games.
bool sl_window_should_fix_randr_emu(struct sl_window* window);

// Get position of the window.
void sl_window_get_x_y(struct sl_window* window, uint32_t* x, uint32_t* y);
// Get size of the window, taking account of emulated status.
void sl_window_get_width_height(struct sl_window* window,
                                uint32_t* w,
                                uint32_t* h);

void sl_window_update_should_be_containerized_from_pid(
    struct sl_window* window);
bool sl_window_is_containerized(struct sl_window* window);
void sl_window_reset_viewport(struct sl_window* window);

// Get the virtual screen position that the window is in. Returns true if mut_x
// and mut_y were updated with the found screen, false otherwise.
bool sl_window_get_output_virt_position(struct sl_window* window,
                                        uint32_t& mut_x,
                                        uint32_t& mut_y);

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_WINDOW_H_
