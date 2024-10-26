// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xcb-shim.h"  // NOLINT(build/include_directory)
#include <xcb/xproto.h>

xcb_connection_t* XcbShim::connect(const char* displayname, int* screenp) {
  return xcb_connect(displayname, screenp);
}

uint32_t XcbShim::generate_id(xcb_connection_t* c) {
  return xcb_generate_id(c);
}

xcb_void_cookie_t XcbShim::create_window(xcb_connection_t* c,
                                         uint8_t depth,
                                         xcb_window_t wid,
                                         xcb_window_t parent,
                                         int16_t x,
                                         int16_t y,
                                         uint16_t width,
                                         uint16_t height,
                                         uint16_t border_width,
                                         uint16_t _class,
                                         xcb_visualid_t visual,
                                         uint32_t value_mask,
                                         const void* value_list) {
  return xcb_create_window(c, depth, wid, parent, x, y, width, height,
                           border_width, _class, visual, value_mask,
                           value_list);
}

xcb_void_cookie_t XcbShim::reparent_window(xcb_connection_t* c,
                                           xcb_window_t window,
                                           xcb_window_t parent,
                                           int16_t x,
                                           int16_t y) {
  return xcb_reparent_window(c, window, parent, x, y);
}

xcb_void_cookie_t XcbShim::map_window(xcb_connection_t* c,
                                      xcb_window_t window) {
  return xcb_map_window(c, window);
}

xcb_void_cookie_t XcbShim::configure_window(xcb_connection_t* c,
                                            xcb_window_t window,
                                            uint16_t value_mask,
                                            const void* value_list) {
  return xcb_configure_window(c, window, value_mask, value_list);
}

xcb_void_cookie_t XcbShim::change_property(xcb_connection_t* c,
                                           uint8_t mode,
                                           xcb_window_t window,
                                           xcb_atom_t property,
                                           xcb_atom_t type,
                                           uint8_t format,
                                           uint32_t data_len,
                                           const void* data) {
  return xcb_change_property(c, mode, window, property, type, format, data_len,
                             data);
}

xcb_void_cookie_t XcbShim::send_event(xcb_connection_t* c,
                                      uint8_t propagate,
                                      xcb_window_t destination,
                                      uint32_t event_mask,
                                      const char* event) {
  return xcb_send_event(c, propagate, destination, event_mask, event);
}

xcb_void_cookie_t XcbShim::change_window_attributes(xcb_connection_t* c,
                                                    xcb_window_t window,
                                                    uint32_t value_mask,
                                                    const void* value_list) {
  return xcb_change_window_attributes(c, window, value_mask, value_list);
}

xcb_get_geometry_cookie_t XcbShim::get_geometry(xcb_connection_t* c,
                                                xcb_drawable_t drawable) {
  return xcb_get_geometry(c, drawable);
}

xcb_get_geometry_reply_t* XcbShim::get_geometry_reply(
    xcb_connection_t* c,
    xcb_get_geometry_cookie_t cookie,
    xcb_generic_error_t** e) {
  return xcb_get_geometry_reply(c, cookie, e);
}

xcb_get_property_cookie_t XcbShim::get_property(xcb_connection_t* c,
                                                uint8_t _delete,
                                                xcb_window_t window,
                                                xcb_atom_t property,
                                                xcb_atom_t type,
                                                uint32_t long_offset,
                                                uint32_t long_length) {
  return xcb_get_property(c, _delete, window, property, type, long_offset,
                          long_length);
}

xcb_get_property_reply_t* XcbShim::get_property_reply(
    xcb_connection_t* c,
    xcb_get_property_cookie_t cookie,
    xcb_generic_error_t** e) {
  return xcb_get_property_reply(c, cookie, e);
}

void* XcbShim::get_property_value(const xcb_get_property_reply_t* r) {
  return xcb_get_property_value(r);
}

int XcbShim::get_property_value_length(const xcb_get_property_reply_t* r) {
  return xcb_get_property_value_length(r);
}

static XcbShim* xcb_singleton = nullptr;

XcbShim* xcb() {
  return xcb_singleton;
}

void set_xcb_shim(XcbShim* shim) {
  xcb_singleton = shim;
}
