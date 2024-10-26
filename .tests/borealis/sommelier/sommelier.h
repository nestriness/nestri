// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_H_

#include <limits.h>
#include <linux/types.h>
#include <sys/types.h>
#include <unordered_map>
#include <wayland-server.h>
#include <wayland-util.h>
#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>

#include "compositor/sommelier-mmap.h"  // NOLINT(build/include_directory)
#include "sommelier-ctx.h"              // NOLINT(build/include_directory)
#include "sommelier-global.h"           // NOLINT(build/include_directory)
#include "sommelier-util.h"             // NOLINT(build/include_directory)
#include "sommelier-window.h"           // NOLINT(build/include_directory)
#include "weak-resource-ptr.h"          // NOLINT(build/include_directory)

#define SOMMELIER_VERSION "0.20"
#define APPLICATION_ID_FORMAT_PREFIX "org.chromium.guest_os.%s"
#define NATIVE_WAYLAND_APPLICATION_ID_FORMAT \
  APPLICATION_ID_FORMAT_PREFIX ".wayland.%s"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define CONTROL_MASK (1 << 0)
#define ALT_MASK (1 << 1)
#define SHIFT_MASK (1 << 2)

struct sl_global;
struct sl_compositor;
struct sl_shm;
struct sl_shell;
struct sl_data_device_manager;
struct sl_data_offer;
struct sl_data_source;
struct sl_xdg_shell;
struct sl_subcompositor;
struct sl_aura_shell;
struct sl_viewporter;
struct sl_linux_dmabuf;
struct sl_keyboard_extension;
struct sl_text_input_extension;
struct sl_text_input_manager;
struct sl_relative_pointer_manager;
struct sl_pointer_constraints;
struct sl_window;
struct sl_host_surface;
struct sl_host_output;
struct sl_fractional_scale_manager;
struct zaura_shell;
struct zcr_keyboard_extension_v1;
struct zxdg_output_manager_v1;

#ifdef GAMEPAD_SUPPORT
struct sl_gamepad;
struct sl_gaming_input_manager;
struct zcr_gaming_input_v2;
struct InputMapping;
#endif

class WaylandChannel;

extern const struct wl_registry_listener sl_registry_listener;

// Exported for testing.
void sl_registry_handler(void* data,
                         struct wl_registry* registry,
                         uint32_t id,
                         const char* interface,
                         uint32_t version);
void sl_registry_remover(void* data, struct wl_registry* registry, uint32_t id);

// We require a host compositor supporting at least this wl_compositor version.
constexpr uint32_t kMinHostWlCompositorVersion =
    WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION;

struct sl_compositor {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
  struct wl_compositor* internal;
};

struct sl_shm {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
  struct wl_shm* internal;
};

struct sl_seat {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_global;
  uint32_t last_serial;
  // Stylus events are received on both wl_touch and stylus interfaces.
  // This is is used to coordinate between the two.
  //
  // TODO(b/281760854): exo should provide this protocol natively
  struct sl_host_stylus_tablet* stylus_tablet;
  struct wl_list link;
};

struct sl_host_pointer {
  struct sl_seat* seat;
  struct wl_resource* resource;
  struct wl_pointer* proxy;
  struct wl_resource* focus_resource;
  struct sl_host_surface* focus_surface;
  struct wl_listener focus_resource_listener;
  uint32_t focus_serial;
  uint32_t time;
  wl_fixed_t axis_delta[2];
  int32_t axis_discrete[2];
};

struct sl_relative_pointer_manager {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

struct sl_viewport {
  struct wl_list link;
  wl_fixed_t src_x;
  wl_fixed_t src_y;
  wl_fixed_t src_width;
  wl_fixed_t src_height;
  int32_t dst_width;
  int32_t dst_height;
};

struct sl_fractional_scale {
  struct wl_list link;
};

struct sl_host_callback {
  struct wl_resource* resource;
  struct wl_callback* proxy;
};

struct sl_host_surface {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_surface* proxy;
  struct wp_viewport* viewport;
  struct wl_buffer* proxy_buffer;

  // Window content size from XWayland: wl_surface.attach(passing a wl_buffer of
  // size w', h').
  uint32_t contents_width;
  uint32_t contents_height;
  uint32_t contents_shm_format;
  int32_t contents_scale;
  int32_t contents_x_offset;
  int32_t contents_y_offset;

  double xdg_scale_x;
  double xdg_scale_y;
  bool scale_round_on_x;
  bool scale_round_on_y;
  struct wl_list contents_viewport;
  struct sl_mmap* contents_shm_mmap;
  bool contents_shaped;
  pixman_region32_t contents_shape;
  int has_role;
  int has_output;
  int has_own_scale;
  int32_t cached_logical_width;
  int32_t cached_logical_height;
  uint32_t last_event_serial;
  struct sl_output_buffer* current_buffer;
  struct zwp_linux_surface_synchronization_v1* surface_sync;
  struct wl_list released_buffers;
  struct wl_list busy_buffers;
  struct sl_window* window = nullptr;
  WeakResourcePtr<sl_host_output> output;
};
MAP_STRUCTS(wl_surface, sl_host_surface);

struct sl_host_region {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_region* proxy;
};
MAP_STRUCTS(wl_region, sl_host_region);

struct sl_host_buffer {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_buffer* proxy;
  uint32_t width;
  uint32_t height;
  struct sl_mmap* shm_mmap;
  uint32_t shm_format;
  struct sl_sync_point* sync_point;
  bool is_drm;
};

struct sl_data_source_send_request {
  int fd;
  xcb_intern_atom_cookie_t cookie;
  struct sl_data_source* data_source;
  struct wl_list link;
};

struct sl_subcompositor {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

struct sl_shell {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

struct sl_output {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_global;
  struct sl_host_output* host_output;
  struct wl_list link;
};

struct sl_xdg_output_manager {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct zxdg_output_manager_v1* internal;
};

struct sl_host_output {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_output* proxy;
  struct zxdg_output_v1* zxdg_output;
  struct zaura_output* aura_output;
  int internal;
  // Whether or not this output has been modified and updated information needs
  // to be forwarded.
  bool needs_update;

  // Position in host's global logical space
  int x;
  int y;

  int physical_width;
  int physical_height;

  // The physical width/height after being scaled by
  // sl_output_get_dimensions_original to adjust the DPI values accordingly.
  int virt_physical_width;
  int virt_physical_height;
  int subpixel;
  char* make;
  char* model;
  int transform;
  uint32_t flags;

  // Physical size in pixels, as indicated by wl_output.mode.
  // https://wayland.app/protocols/wayland#wl_output:event:mode
  int width;
  int height;

  // The width/height after being scaled.
  int virt_width;
  int virt_height;

  // The width/height after being scaled by
  // sl_output_get_dimensions_original and rotated.
  int virt_rotated_width;
  int virt_rotated_height;

  int refresh;

  // The output scale received from the host via wl_output.scale.
  // When run without aura-shell and in non-direct-scale mode,
  // we forward this scale to clients.
  int scale_factor;
  // The current scale being used which is provided by zaura_output.scale.
  int current_scale;
  // The preferred scale which is provided by zaura_output.scale. On a
  // Chromebook, this is the "Default" value in Settings>Device>Display.
  int preferred_scale;
  // The device_scale_factor provided by zaura_output.device_scale_factor.
  // Chromebooks come with their own default device scale which is appropriate
  // for their screen sizes.
  int device_scale_factor;
  int expecting_scale;
  bool expecting_logical_size;

  // xdg_output values.
  // Ref: https://wayland.app/protocols/xdg-output-unstable-v1
  int32_t logical_width;
  int32_t logical_height;
  int32_t logical_x;
  int32_t logical_y;

  // The scaling factors for direct mode
  // virt_scale: Used to translate from physical space to virtual space
  // xdg_scale: Used to translate from virtual space to logical space
  //
  // The logical space is defined by the host. It will be retrieved through
  // the xdg_output_manager interface.
  //
  // All spaces, and by consequence all scale factors, will be unique to each
  // particular output.
  //
  // For more details, see sommelier-transform.h

  double virt_scale_x;
  double virt_scale_y;
  double xdg_scale_x;
  double xdg_scale_y;
  int virt_x;
  // Note that all virtual screens are repositioned laterally in a row
  // (regardless of host's positioning), which means y coordinate is always 0.
  // However, in case future implementation changes, utilize this attribute
  // whenever we reference virtual y coordinates.
  int virt_y;
};
MAP_STRUCTS(wl_output, sl_host_output);

struct sl_host_seat {
  struct sl_seat* seat;
  struct wl_resource* resource;
  struct wl_seat* proxy;
};
MAP_STRUCTS(wl_seat, sl_host_seat);

struct sl_accelerator {
  uint32_t modifiers;
  xkb_keysym_t symbol;
};

struct sl_keyboard_extension {
  struct sl_context* ctx;
  uint32_t id;
  struct zcr_keyboard_extension_v1* internal;
};

struct sl_data_device_manager {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_global;
  struct wl_data_device_manager* internal;
};

struct sl_data_offer {
  struct sl_context* ctx;
  struct wl_data_offer* internal;
  struct wl_array atoms;    // Contains xcb_atom_t
  struct wl_array cookies;  // Contains xcb_intern_atom_cookie_t
};

struct sl_text_input_manager {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
  struct sl_global* host_crostini_manager_global;
  struct sl_global* host_x11_global;
};

struct sl_text_input_extension {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

#ifdef GAMEPAD_SUPPORT
struct sl_gaming_input_manager {
  struct sl_context* ctx;
  uint32_t id;
  struct zcr_gaming_input_v2* internal;
};
#endif

struct sl_stylus_input_manager {
  struct sl_context* ctx;
  uint32_t id;
  struct zcr_stylus_v2* internal;
  struct sl_global* tablet_host_global;
};

struct sl_pointer_constraints {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

struct sl_viewporter {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_viewporter_global;
  struct wp_viewporter* internal;
};

struct sl_xdg_shell {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_global;
  struct xdg_wm_base* internal;
};

struct sl_aura_shell {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_gtk_shell_global;
  struct zaura_shell* internal;
};

/* Support for zwp_linux_dmabuf_v1 is expected of the server and advertised at
 * the same version to clients (up to SL_LINUX_DMABUF_MAX_VERSION).
 *
 * When used directly by a client, each client request is passed through to the
 * server (with some having additional internal behaviors). In such cases, the
 * bound global is used to create proxies on-demand.
 *
 * There are additional internal uses for zwp_linux_dmabuf_v1:
 *   - Emulating client wl_drm requests as zwp_linux_dmabuf_v1 requests to the
 *     server. Each client-facing wl_drm resource instantiates its own
 *     zwp_linux_dmabuf_v1 (version 2) server proxy to use internally.
 *   - Creating host-sharable dmabufs (via the virtualization channel) as
 *     "copy-on-commit" targets for client wl_shm buffers. A server-facing
 *     zwp_linux_dmabuf_v1 (version 2) proxy is created to facilitate wl_buffer
 *     creation from these dmabufs.
 *   - Querying and converting the list of supported dmabuf formats to wl_shm
 *     formats to send to a client upon wl_shm binding. Each client-facing
 *     wl_shm resource instantiates its own zwp_linux_dmabuf_v1 (version 1)
 *     proxy to use internally.
 */
struct sl_linux_dmabuf {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_drm_global;
  struct sl_global* host_linux_dmabuf_global;

  // binding (version 2) is only used for wl_shm copy-on-commit
  struct zwp_linux_dmabuf_v1* proxy_v2;
};

struct sl_linux_explicit_synchronization {
  struct sl_context* ctx;
  uint32_t id;
  struct zwp_linux_explicit_synchronization_v1* internal;
};

struct sl_fractional_scale_manager {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_fractional_scale_manager_global;
  struct wp_fractional_scale_manager_v1* internal;
};

struct sl_global {
  struct sl_context* ctx;
  const struct wl_interface* interface;
  uint32_t name;
  uint32_t version;
  void* data;
  wl_global_bind_func_t bind;
  struct wl_list link;
};

struct sl_host_registry {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_list link;
};

typedef void (*sl_sync_func_t)(struct sl_context* ctx,
                               struct sl_sync_point* sync_point);

struct sl_sync_point {
  int fd;
  sl_sync_func_t sync;
};

#ifdef GAMEPAD_SUPPORT
struct InputMapping {
  const char* id;
  const std::unordered_map<uint32_t, uint32_t> mapping;
};

struct sl_host_gaming_seat {
  struct sl_host_gaming_seat* gaming_seat;
  struct wl_resource* resource;
  struct wl_seat* proxy;
};

struct sl_host_gamepad {
  struct sl_context* ctx;
  int state;
  struct libevdev* ev_dev;
  struct libevdev_uinput* uinput_dev;
  struct wl_list link;
  const char* name;
  uint32_t bus;
  uint32_t vendor_id;
  uint32_t product_id;
  uint32_t version;
  const InputMapping* input_mapping;
};
#endif

struct sl_idle_inhibit_manager {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

struct sl_host_buffer* sl_create_host_buffer(struct sl_context* ctx,
                                             struct wl_client* client,
                                             uint32_t id,
                                             struct wl_buffer* proxy,
                                             int32_t width,
                                             int32_t height,
                                             bool is_drm);

struct sl_global* sl_compositor_global_create(struct sl_context* ctx);
void sl_compositor_init_context(struct sl_context* ctx,
                                struct wl_registry* registry,
                                uint32_t id,
                                uint32_t version);

struct sl_global* sl_shm_global_create(struct sl_context* ctx);

struct sl_global* sl_subcompositor_global_create(struct sl_context* ctx);

struct sl_global* sl_shell_global_create(struct sl_context* ctx);

struct sl_global* sl_stylus_to_tablet_manager_global_create(
    struct sl_context* ctx);

double sl_output_aura_scale_factor_to_double(int scale_factor);

// Given a position in global host logical space (see sommelier-transform.h),
// return the output that contains it, or the closest output.
struct sl_host_output* sl_infer_output_for_host_position(struct sl_context* ctx,
                                                         int32_t host_x,
                                                         int32_t host_y);

// Given a position in virtual coordinate space (see sommelier-transform.h),
// return the output that contains it, or the closest output.
struct sl_host_output* sl_infer_output_for_guest_position(
    struct sl_context* ctx, int32_t virt_x, int32_t virt_y);

// Generates all the virtual/modified values to be forwarded to the client.
void sl_output_calculate_virtual_dimensions(struct sl_host_output* host);

// Updates the virt_x of all outputs.
void sl_output_update_output_x(struct sl_context* ctx);

// Forwards all the available information of an output to the client.
void sl_output_send_host_output_state(struct sl_host_output* host);

struct sl_global* sl_output_global_create(struct sl_output* output);

struct sl_global* sl_seat_global_create(struct sl_seat* seat);

struct sl_global* sl_relative_pointer_manager_global_create(
    struct sl_context* ctx);

struct sl_global* sl_data_device_manager_global_create(struct sl_context* ctx);

struct sl_global* sl_viewporter_global_create(struct sl_context* ctx);

struct sl_global* sl_xdg_shell_global_create(struct sl_context* ctx,
                                             uint32_t version);

struct sl_global* sl_gtk_shell_global_create(struct sl_context* ctx);

struct sl_global* sl_drm_global_create(struct sl_context* ctx,
                                       struct sl_linux_dmabuf* linux_dmabuf);

struct sl_global* sl_linux_dmabuf_global_create(
    struct sl_context* ctx, struct sl_linux_dmabuf* linux_dmabuf);

struct sl_global* sl_text_input_extension_global_create(struct sl_context* ctx,
                                                        uint32_t exo_version);

struct sl_global* sl_text_input_manager_global_create(struct sl_context* ctx,
                                                      uint32_t exo_version);

struct sl_global* sl_text_input_x11_global_create(struct sl_context* ctx);
struct sl_global* sl_text_input_crostini_manager_global_create(
    struct sl_context* ctx);

struct sl_global* sl_pointer_constraints_global_create(struct sl_context* ctx);

struct sl_global* sl_fractional_scale_manager_global_create(
    struct sl_context* ctx);

struct sl_global* sl_idle_inhibit_manager_global_create(struct sl_context* ctx);

void sl_set_display_implementation(struct sl_context* ctx,
                                   struct wl_client* client);

struct sl_sync_point* sl_sync_point_create(int fd);
void sl_sync_point_destroy(struct sl_sync_point* sync_point);

void sl_host_seat_added(struct sl_host_seat* host);
void sl_host_seat_removed(struct sl_host_seat* host);

void sl_restack_windows(struct sl_context* ctx, uint32_t focus_resource_id);

void sl_roundtrip(struct sl_context* ctx);

struct sl_window* sl_lookup_window(struct sl_context* ctx, xcb_window_t id);
int sl_is_our_window(struct sl_context* ctx, xcb_window_t id);

// Exported for testing
void sl_handle_destroy_notify(struct sl_context* ctx,
                              xcb_destroy_notify_event_t* event);
void sl_handle_reparent_notify(struct sl_context* ctx,
                               xcb_reparent_notify_event_t* event);
void sl_handle_map_request(struct sl_context* ctx,
                           xcb_map_request_event_t* event);
void sl_handle_unmap_notify(struct sl_context* ctx,
                            xcb_unmap_notify_event_t* event);
void sl_handle_configure_request(struct sl_context* ctx,
                                 xcb_configure_request_event_t* event);
void sl_handle_property_notify(struct sl_context* ctx,
                               xcb_property_notify_event_t* event);
void sl_create_window(struct sl_context* ctx,
                      xcb_window_t id,
                      int x,
                      int y,
                      int width,
                      int height,
                      int border_width);
void sl_handle_client_message(struct sl_context* ctx,
                              xcb_client_message_event_t* event);
void sl_handle_focus_in(struct sl_context* ctx, xcb_focus_in_event_t* event);

void sl_host_surface_commit(struct wl_client* client,
                            struct wl_resource* resource);

#ifdef GAMEPAD_SUPPORT
void sl_gaming_seat_add_listener(struct sl_context* ctx);
#endif

bool sl_client_supports_interface(const sl_context* ctx,
                                  const wl_client* client,
                                  const wl_interface* interface);

#define sl_array_for_each(pos, array)                                   \
  for (pos = static_cast<decltype(pos)>((array)->data);                 \
       (const char*)pos < ((const char*)(array)->data + (array)->size); \
       (pos)++)

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_H_
