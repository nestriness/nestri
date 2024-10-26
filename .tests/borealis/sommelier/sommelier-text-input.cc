// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"            // NOLINT(build/include_directory)
#include "sommelier-transform.h"  // NOLINT(build/include_directory)
#include "weak-resource-ptr.h"    // NOLINT(build/include_directory)

#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "text-input-crostini-unstable-v1-server-protocol.h"  // NOLINT(build/include_directory)
#include "text-input-extension-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "text-input-extension-unstable-v1-server-protocol.h"  // NOLINT(build/include_directory)
#include "text-input-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "text-input-unstable-v1-server-protocol.h"  // NOLINT(build/include_directory)
#include "text-input-x11-unstable-v1-server-protocol.h"  // NOLINT(build/include_directory)

namespace {

// Versions supported by sommelier.
constexpr uint32_t kTextInputManagerVersion = 1;
constexpr uint32_t kTextInputExtensionVersion = 11;
constexpr uint32_t kTextInputCrostiniManagerVersion = 1;
constexpr uint32_t kTextInputX11Version = 1;

}  // namespace

struct sl_host_text_input_manager {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct zwp_text_input_manager_v1* proxy;
};

struct sl_host_text_input {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct zwp_text_input_v1* proxy;

  struct wl_resource* text_input_crostini_resource;

  WeakResourcePtr<sl_host_surface> active_surface;
};
MAP_STRUCTS(zwp_text_input_v1, sl_host_text_input);

struct sl_host_text_input_extension {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct zcr_text_input_extension_v1* proxy;
};

struct sl_host_extended_text_input {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct sl_host_text_input* host_text_input;
  struct zcr_extended_text_input_v1* proxy;
};
MAP_STRUCTS(zcr_extended_text_input_v1, sl_host_extended_text_input);

////////////////////////////////////////////////////////////////////////////////
// zwp_text_input_v1 implementation

static void sl_text_input_activate(wl_client* client,
                                   wl_resource* resource,
                                   wl_resource* seat,
                                   wl_resource* surface) {
  struct sl_host_text_input* host =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(resource));
  struct sl_host_seat* host_seat =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(seat));
  struct sl_host_surface* host_surface =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(surface));

  host->active_surface = host_surface;
  zwp_text_input_v1_activate(host->proxy, host_seat->proxy,
                             host_surface->proxy);
}

static void sl_text_input_deactivate(wl_client* client,
                                     wl_resource* resource,
                                     wl_resource* seat) {
  struct sl_host_text_input* host =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(resource));
  struct sl_host_seat* host_seat =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(seat));

  host->active_surface.Reset();

  zwp_text_input_v1_deactivate(host->proxy, host_seat->proxy);
}

static void sl_text_input_set_cursor_rectangle(wl_client* client,
                                               wl_resource* resource,
                                               int32_t x,
                                               int32_t y,
                                               int32_t width,
                                               int32_t height) {
  struct sl_host_text_input* host =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(resource));

  if (!host->active_surface)
    return;

  int32_t x1 = x;
  int32_t y1 = y;
  int32_t x2 = x + width;
  int32_t y2 = y + height;

  sl_transform_guest_to_host(host->ctx, host->active_surface.get(), &x1, &y1);
  sl_transform_guest_to_host(host->ctx, host->active_surface.get(), &x2, &y2);

  zwp_text_input_v1_set_cursor_rectangle(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static const struct zwp_text_input_v1_interface sl_text_input_implementation = {
    sl_text_input_activate,
    sl_text_input_deactivate,
    ForwardRequest<zwp_text_input_v1_show_input_panel>,
    ForwardRequest<zwp_text_input_v1_hide_input_panel>,
    ForwardRequest<zwp_text_input_v1_reset>,
    ForwardRequest<zwp_text_input_v1_set_surrounding_text>,
    ForwardRequest<zwp_text_input_v1_set_content_type>,
    sl_text_input_set_cursor_rectangle,
    ForwardRequest<zwp_text_input_v1_set_preferred_language>,
    ForwardRequest<zwp_text_input_v1_commit_state>,
    ForwardRequest<zwp_text_input_v1_invoke_action>,
};

static void sl_text_input_enter(void* data,
                                struct zwp_text_input_v1* text_input,
                                struct wl_surface* surface) {
  // This is not currently used by cros_im. We can't simply forward the event
  // as for an x11-hosted cros_im instance the text_input and wl_surface
  // objects will be on different clients. We could add a corresponding event
  // to zcr_text_input_crostini if needed.
}

static void sl_text_input_leave(void* data,
                                struct zwp_text_input_v1* text_input) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_leave(host->resource);
}

static void sl_text_input_modifiers_map(void* data,
                                        struct zwp_text_input_v1* text_input,
                                        struct wl_array* map) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_modifiers_map(host->resource, map);
}

static void sl_text_input_input_panel_state(
    void* data, struct zwp_text_input_v1* text_input, uint32_t state) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_input_panel_state(host->resource, state);
}

static void sl_text_input_preedit_string(void* data,
                                         struct zwp_text_input_v1* text_input,
                                         uint32_t serial,
                                         const char* text,
                                         const char* commit) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_preedit_string(host->resource, serial, text, commit);
}

static void sl_text_input_preedit_styling(void* data,
                                          struct zwp_text_input_v1* text_input,
                                          uint32_t index,
                                          uint32_t length,
                                          uint32_t style) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_preedit_styling(host->resource, index, length, style);
}

static void sl_text_input_preedit_cursor(void* data,
                                         struct zwp_text_input_v1* text_input,
                                         int32_t index) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_preedit_cursor(host->resource, index);
}

static void sl_text_input_commit_string(void* data,
                                        struct zwp_text_input_v1* text_input,
                                        uint32_t serial,
                                        const char* text) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_commit_string(host->resource, serial, text);
}

static void sl_text_input_cursor_position(void* data,
                                          struct zwp_text_input_v1* text_input,
                                          int32_t index,
                                          int32_t anchor) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_cursor_position(host->resource, index, anchor);
}

static void sl_text_input_delete_surrounding_text(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    uint32_t length) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_delete_surrounding_text(host->resource, index, length);
}

static void sl_text_input_keysym(void* data,
                                 struct zwp_text_input_v1* text_input,
                                 uint32_t serial,
                                 uint32_t time,
                                 uint32_t sym,
                                 uint32_t state,
                                 uint32_t modifiers) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_keysym(host->resource, serial, time, sym, state,
                                modifiers);
}

static void sl_text_input_language(void* data,
                                   struct zwp_text_input_v1* text_input,
                                   uint32_t serial,
                                   const char* language) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_language(host->resource, serial, language);
}

static void sl_text_input_text_direction(void* data,
                                         struct zwp_text_input_v1* text_input,
                                         uint32_t serial,
                                         uint32_t direction) {
  struct sl_host_text_input* host = static_cast<sl_host_text_input*>(
      zwp_text_input_v1_get_user_data(text_input));

  zwp_text_input_v1_send_text_direction(host->resource, serial, direction);
}

static const struct zwp_text_input_v1_listener sl_text_input_listener = {
    sl_text_input_enter,           sl_text_input_leave,
    sl_text_input_modifiers_map,   sl_text_input_input_panel_state,
    sl_text_input_preedit_string,  sl_text_input_preedit_styling,
    sl_text_input_preedit_cursor,  sl_text_input_commit_string,
    sl_text_input_cursor_position, sl_text_input_delete_surrounding_text,
    sl_text_input_keysym,          sl_text_input_language,
    sl_text_input_text_direction,
};

static void sl_destroy_host_text_input(struct wl_resource* resource) {
  struct sl_host_text_input* host =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(resource));

  zwp_text_input_v1_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

////////////////////////////////////////////////////////////////////////////////
// zwp_text_input_manager_v1 implementation

static void sl_text_input_manager_create_text_input(
    struct wl_client* client, struct wl_resource* resource, uint32_t id) {
  struct sl_host_text_input_manager* host =
      static_cast<sl_host_text_input_manager*>(
          wl_resource_get_user_data(resource));
  struct wl_resource* text_input_resource =
      wl_resource_create(client, &zwp_text_input_v1_interface,
                         wl_resource_get_version(resource), id);
  struct sl_host_text_input* text_input_host = new sl_host_text_input();

  text_input_host->resource = text_input_resource;
  text_input_host->ctx = host->ctx;
  text_input_host->proxy =
      zwp_text_input_manager_v1_create_text_input(host->proxy);
  wl_resource_set_implementation(text_input_resource,
                                 &sl_text_input_implementation, text_input_host,
                                 sl_destroy_host_text_input);
  zwp_text_input_v1_add_listener(text_input_host->proxy,
                                 &sl_text_input_listener, text_input_host);
}

static void sl_destroy_host_text_input_manager(struct wl_resource* resource) {
  struct sl_host_text_input_manager* host =
      static_cast<sl_host_text_input_manager*>(
          wl_resource_get_user_data(resource));

  zwp_text_input_manager_v1_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static struct zwp_text_input_manager_v1_interface
    sl_text_input_manager_implementation = {
        sl_text_input_manager_create_text_input,
};

static void sl_bind_host_text_input_manager(struct wl_client* client,
                                            void* data,
                                            uint32_t app_version,
                                            uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_text_input_manager* text_input_manager = ctx->text_input_manager;
  struct sl_host_text_input_manager* host = new sl_host_text_input_manager();
  host->ctx = ctx;
  host->resource = wl_resource_create(
      client, &zwp_text_input_manager_v1_interface, app_version, id);
  wl_resource_set_implementation(host->resource,
                                 &sl_text_input_manager_implementation, host,
                                 sl_destroy_host_text_input_manager);
  host->proxy = static_cast<zwp_text_input_manager_v1*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), text_input_manager->id,
      &zwp_text_input_manager_v1_interface,
      wl_resource_get_version(host->resource)));
  zwp_text_input_manager_v1_set_user_data(host->proxy, host);
}

struct sl_global* sl_text_input_manager_global_create(struct sl_context* ctx,
                                                      uint32_t exo_version) {
  return sl_global_create(ctx, &zwp_text_input_manager_v1_interface,
                          std::min(exo_version, kTextInputManagerVersion), ctx,
                          sl_bind_host_text_input_manager);
}

////////////////////////////////////////////////////////////////////////////////
// zcr_extended_text_input_v1 implementation

static void sl_extended_text_input_destroy(struct wl_client* client,
                                           struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct zcr_extended_text_input_v1_interface
    sl_extended_text_input_implementation = {
        sl_extended_text_input_destroy,
        ForwardRequest<zcr_extended_text_input_v1_deprecated_set_input_type>,
        ForwardRequest<
            zcr_extended_text_input_v1_set_grammar_fragment_at_cursor>,
        ForwardRequest<zcr_extended_text_input_v1_set_autocorrect_info>,
        ForwardRequest<
            zcr_extended_text_input_v1_finalize_virtual_keyboard_changes>,
        ForwardRequest<zcr_extended_text_input_v1_set_focus_reason>,
        ForwardRequest<zcr_extended_text_input_v1_set_input_type>,
        ForwardRequest<zcr_extended_text_input_v1_set_surrounding_text_support>,
        ForwardRequest<
            zcr_extended_text_input_v1_set_surrounding_text_offset_utf16>,
};

static void sl_extended_text_input_set_preedit_region(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    int32_t index,
    uint32_t length) {
  struct sl_host_extended_text_input* host =
      static_cast<sl_host_extended_text_input*>(
          zcr_extended_text_input_v1_get_user_data(extended_text_input));

  zcr_extended_text_input_v1_send_set_preedit_region(host->resource, index,
                                                     length);
}

static void sl_extended_text_input_clear_grammar_fragments(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t start,
    uint32_t end) {
  struct sl_host_extended_text_input* host =
      static_cast<sl_host_extended_text_input*>(
          zcr_extended_text_input_v1_get_user_data(extended_text_input));

  zcr_extended_text_input_v1_send_clear_grammar_fragments(host->resource, start,
                                                          end);
}

static void sl_extended_text_input_add_grammar_fragment(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t start,
    uint32_t end,
    const char* suggestion) {
  struct sl_host_extended_text_input* host =
      static_cast<sl_host_extended_text_input*>(
          zcr_extended_text_input_v1_get_user_data(extended_text_input));

  zcr_extended_text_input_v1_send_add_grammar_fragment(host->resource, start,
                                                       end, suggestion);
}

static void sl_extended_text_input_set_autocorrect_range(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t start,
    uint32_t end) {
  struct sl_host_extended_text_input* host =
      static_cast<sl_host_extended_text_input*>(
          zcr_extended_text_input_v1_get_user_data(extended_text_input));

  zcr_extended_text_input_v1_send_set_autocorrect_range(host->resource, start,
                                                        end);
}

static void sl_extended_text_input_set_virtual_keyboard_occluded_bounds(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {
  struct sl_host_extended_text_input* host =
      static_cast<sl_host_extended_text_input*>(
          zcr_extended_text_input_v1_get_user_data(extended_text_input));

  zcr_extended_text_input_v1_send_set_virtual_keyboard_occluded_bounds(
      host->resource, x, y, width, height);
}

static void sl_extended_text_input_confirm_preedit(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    uint32_t selection_behaviour) {
  struct sl_host_extended_text_input* host =
      static_cast<sl_host_extended_text_input*>(
          zcr_extended_text_input_v1_get_user_data(extended_text_input));

  zcr_extended_text_input_v1_send_confirm_preedit(host->resource,
                                                  selection_behaviour);
}

static const struct zcr_extended_text_input_v1_listener
    sl_extended_text_input_listener = {
        sl_extended_text_input_set_preedit_region,
        sl_extended_text_input_clear_grammar_fragments,
        sl_extended_text_input_add_grammar_fragment,
        sl_extended_text_input_set_autocorrect_range,
        sl_extended_text_input_set_virtual_keyboard_occluded_bounds,
        sl_extended_text_input_confirm_preedit,
};

static void sl_destroy_host_extended_text_input(struct wl_resource* resource) {
  struct sl_host_extended_text_input* host =
      static_cast<sl_host_extended_text_input*>(
          wl_resource_get_user_data(resource));

  zcr_extended_text_input_v1_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

////////////////////////////////////////////////////////////////////////////////
// zcr_text_input_extension_v1 implementation

static void sl_text_input_extension_get_extended_text_input(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* text_input) {
  struct sl_host_text_input_extension* host =
      static_cast<sl_host_text_input_extension*>(
          wl_resource_get_user_data(resource));
  struct sl_host_text_input* host_text_input =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(text_input));
  struct wl_resource* extended_text_input_resource =
      wl_resource_create(client, &zcr_extended_text_input_v1_interface,
                         wl_resource_get_version(resource), id);
  struct sl_host_extended_text_input* extended_text_input_host =
      new sl_host_extended_text_input();

  extended_text_input_host->resource = extended_text_input_resource;
  extended_text_input_host->ctx = host->ctx;
  extended_text_input_host->proxy =
      zcr_text_input_extension_v1_get_extended_text_input(
          host->proxy, host_text_input->proxy);
  wl_resource_set_implementation(
      extended_text_input_resource, &sl_extended_text_input_implementation,
      extended_text_input_host, sl_destroy_host_extended_text_input);
  zcr_extended_text_input_v1_add_listener(extended_text_input_host->proxy,
                                          &sl_extended_text_input_listener,
                                          extended_text_input_host);
}

static void sl_destroy_host_text_input_extension(struct wl_resource* resource) {
  struct sl_host_text_input_extension* host =
      static_cast<sl_host_text_input_extension*>(
          wl_resource_get_user_data(resource));

  zcr_text_input_extension_v1_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static struct zcr_text_input_extension_v1_interface
    sl_text_input_extension_implementation = {
        sl_text_input_extension_get_extended_text_input,
};

static void sl_bind_host_text_input_extension(struct wl_client* client,
                                              void* data,
                                              uint32_t app_version,
                                              uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_text_input_extension* text_input_extension =
      ctx->text_input_extension;
  struct sl_host_text_input_extension* host =
      new sl_host_text_input_extension();
  host->ctx = ctx;
  host->resource = wl_resource_create(
      client, &zcr_text_input_extension_v1_interface, app_version, id);
  wl_resource_set_implementation(host->resource,
                                 &sl_text_input_extension_implementation, host,
                                 sl_destroy_host_text_input_extension);
  host->proxy = static_cast<zcr_text_input_extension_v1*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), text_input_extension->id,
      &zcr_text_input_extension_v1_interface,
      wl_resource_get_version(host->resource)));
  zcr_text_input_extension_v1_set_user_data(host->proxy, host);
}

struct sl_global* sl_text_input_extension_global_create(struct sl_context* ctx,
                                                        uint32_t exo_version) {
  return sl_global_create(ctx, &zcr_text_input_extension_v1_interface,
                          std::min(exo_version, kTextInputExtensionVersion),
                          ctx, sl_bind_host_text_input_extension);
}

////////////////////////////////////////////////////////////////////////////////
// zcr_text_input_crostini implementation

static void sl_text_input_crostini_destroy(struct wl_client* client,
                                           struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

// Used by both text_input_crostini and (deprecated) text_input_x11.
// |resource| is a zcr_text_input_crostini in the former and a zwp_text_input
// in the latter, in either case the user data is the sl_host_text_input.
static void sl_text_input_crostini_activate_x11(wl_client* client,
                                                wl_resource* resource,
                                                wl_resource* seat,
                                                uint32_t x11_window_id) {
  struct sl_host_text_input* host_text_input =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(resource));
  struct sl_host_seat* host_seat =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(seat));
  assert(host_text_input);
  assert(host_seat);

  struct sl_context* ctx = host_text_input->ctx;

  struct sl_window* window;
  wl_list_for_each(window, &ctx->windows, link) {
    if (window->id != x11_window_id)
      continue;
    if (!window->host_surface_id)
      return;
    struct wl_resource* host_window_resource =
        wl_client_get_object(ctx->client, window->host_surface_id);
    if (!host_window_resource)
      return;
    sl_host_surface* host_surface = static_cast<sl_host_surface*>(
        wl_resource_get_user_data(host_window_resource));
    host_text_input->active_surface = host_surface;
    zwp_text_input_v1_activate(host_text_input->proxy, host_seat->proxy,
                               host_surface->proxy);
    return;
  }
}

static const struct zcr_text_input_crostini_v1_interface
    sl_text_input_crostini_implementation = {
        sl_text_input_crostini_destroy,
        sl_text_input_crostini_activate_x11,
};

static void sl_destroy_host_text_input_crostini(struct wl_resource* resource) {
  struct sl_host_text_input* host =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(resource));

  host->text_input_crostini_resource = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// zcr_text_input_crostini_manager implementation

static void sl_text_input_crostini_manager_get_text_input_crostini(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* text_input) {
  struct sl_host_text_input* host_text_input =
      static_cast<sl_host_text_input*>(wl_resource_get_user_data(text_input));

  if (host_text_input->text_input_crostini_resource) {
    wl_resource_post_error(
        resource,
        ZCR_TEXT_INPUT_CROSTINI_MANAGER_V1_ERROR_TEXT_INPUT_CROSTINI_EXISTS,
        "text_input has already been associated with a text_input_crostini "
        "object");
    return;
  }

  wl_resource* text_input_crostini_resource =
      wl_resource_create(client, &zcr_text_input_crostini_v1_interface,
                         wl_resource_get_version(resource), id);
  host_text_input->text_input_crostini_resource = text_input_crostini_resource;

  wl_resource_set_implementation(
      text_input_crostini_resource, &sl_text_input_crostini_implementation,
      host_text_input, sl_destroy_host_text_input_crostini);
}

static struct zcr_text_input_crostini_manager_v1_interface
    sl_text_input_crostini_manager_implementation = {
        sl_text_input_crostini_manager_get_text_input_crostini,
};

static void sl_bind_host_text_input_crostini_manager(struct wl_client* client,
                                                     void* data,
                                                     uint32_t app_version,
                                                     uint32_t id) {
  // This exists only between sommelier and its clients and there is no proxy
  // to the host. We don't use a sl_host_text_input_crostini_manager type as it
  // is not needed.
  wl_resource* resource = wl_resource_create(
      client, &zcr_text_input_crostini_manager_v1_interface, app_version, id);
  wl_resource_set_implementation(resource,
                                 &sl_text_input_crostini_manager_implementation,
                                 nullptr, nullptr);
}

struct sl_global* sl_text_input_crostini_manager_global_create(
    struct sl_context* ctx) {
  return sl_global_create(ctx, &zcr_text_input_crostini_manager_v1_interface,
                          kTextInputCrostiniManagerVersion, ctx,
                          sl_bind_host_text_input_crostini_manager);
}

////////////////////////////////////////////////////////////////////////////////
// zcr_text_input_x11_v1 implementation
// TODO(b/295265862): Deprecated, remove some time after M122.

static void sl_text_input_x11_activate(wl_client* client,
                                       wl_resource* resource,
                                       wl_resource* text_input,
                                       wl_resource* seat,
                                       uint32_t x11_window_id) {
  return sl_text_input_crostini_activate_x11(client, text_input, seat,
                                             x11_window_id);
}

static const struct zcr_text_input_x11_v1_interface
    sl_text_input_x11_implementation = {
        sl_text_input_x11_activate,
};

static void sl_bind_host_text_input_x11(struct wl_client* client,
                                        void* data,
                                        uint32_t app_version,
                                        uint32_t id) {
  // This exists only between sommelier and its clients and there is no proxy
  // to the host. For simplicity we don't use a sl_host_text_input_x11
  // type as it is not needed.
  wl_resource* resource = wl_resource_create(
      client, &zcr_text_input_x11_v1_interface, app_version, id);
  wl_resource_set_implementation(resource, &sl_text_input_x11_implementation,
                                 nullptr, nullptr);
}

struct sl_global* sl_text_input_x11_global_create(struct sl_context* ctx) {
  return sl_global_create(ctx, &zcr_text_input_x11_v1_interface,
                          kTextInputX11Version, ctx,
                          sl_bind_host_text_input_x11);
}
