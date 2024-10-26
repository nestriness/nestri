// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <utility>

#include "fake-xcb-shim.h"  // NOLINT(build/include_directory)

xcb_connection_t* FakeXcbShim::connect(const char* displayname, int* screenp) {
  return nullptr;
}

uint32_t FakeXcbShim::generate_id(xcb_connection_t* c) {
  return next_id_++;
}

xcb_void_cookie_t FakeXcbShim::create_window(xcb_connection_t* c,
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
  std::pair<std::unordered_map<xcb_window_t, FakeWindow>::iterator, bool>
      result = windows_.try_emplace(wid, FakeWindow{
                                             .depth = depth,
                                             .wid = wid,
                                             .parent = parent,
                                             .x = x,
                                             .y = y,
                                             .width = width,
                                             .height = height,
                                             .border_width = border_width,
                                             ._class = _class,
                                             .visual = visual,
                                         });
  // Window should not already exist.
  EXPECT_TRUE(result.second);
  return {};
}

xcb_void_cookie_t FakeXcbShim::reparent_window(xcb_connection_t* c,
                                               xcb_window_t window,
                                               xcb_window_t parent,
                                               int16_t x,
                                               int16_t y) {
  windows_.at(window).parent = parent;
  return {};
}

xcb_void_cookie_t FakeXcbShim::map_window(xcb_connection_t* c,
                                          xcb_window_t window) {
  windows_.at(window).mapped = true;
  return {};
}

xcb_void_cookie_t FakeXcbShim::configure_window(xcb_connection_t* c,
                                                xcb_window_t window,
                                                uint16_t value_mask,
                                                const void* value_list) {
  ADD_FAILURE() << "unimplemented";
  return {};
}

xcb_void_cookie_t FakeXcbShim::change_property(xcb_connection_t* c,
                                               uint8_t mode,
                                               xcb_window_t window,
                                               xcb_atom_t property,
                                               xcb_atom_t type,
                                               uint8_t format,
                                               uint32_t data_len,
                                               const void* data) {
  // prepend/append not implemented
  EXPECT_EQ(mode, XCB_PROP_MODE_REPLACE);

  // The real API returns BadWindow errors if an invalid window ID is passed,
  // but throwing an exception is sufficient for the purposes of our tests.
  uint32_t bytes_per_element = format / 8;
  windows_.at(window).properties_[property] = FakeProperty{
      .type = type,
      .format = format,
      .data = std::vector<unsigned char>(
          (unsigned char*)data,
          (unsigned char*)data + data_len * bytes_per_element),
  };
  return {};
}

xcb_void_cookie_t FakeXcbShim::send_event(xcb_connection_t* c,
                                          uint8_t propagate,
                                          xcb_window_t destination,
                                          uint32_t event_mask,
                                          const char* event) {
  ADD_FAILURE() << "unimplemented";
  return {};
}

xcb_void_cookie_t FakeXcbShim::change_window_attributes(
    xcb_connection_t* c,
    xcb_window_t window,
    uint32_t value_mask,
    const void* value_list) {
  ADD_FAILURE() << "unimplemented";
  return {};
}

xcb_get_geometry_cookie_t FakeXcbShim::get_geometry(xcb_connection_t* c,
                                                    xcb_drawable_t drawable) {
  ADD_FAILURE() << "unimplemented";
  return {};
}

xcb_get_geometry_reply_t* FakeXcbShim::get_geometry_reply(
    xcb_connection_t* c,
    xcb_get_geometry_cookie_t cookie,
    xcb_generic_error_t** e) {
  ADD_FAILURE() << "unimplemented";
  return {};
}

xcb_get_property_cookie_t FakeXcbShim::get_property(xcb_connection_t* c,
                                                    uint8_t _delete,
                                                    xcb_window_t window,
                                                    xcb_atom_t property,
                                                    xcb_atom_t type,
                                                    uint32_t long_offset,
                                                    uint32_t long_length) {
  xcb_get_property_cookie_t cookie;
  cookie.sequence = next_cookie_++;

  PropertyRequestData request;
  request.window = window;
  request.property = property;

  property_requests_[cookie.sequence] = request;
  return cookie;
}

xcb_get_property_reply_t* FakeXcbShim::get_property_reply(
    xcb_connection_t* c,
    xcb_get_property_cookie_t cookie,
    xcb_generic_error_t** e) {
  auto w = windows_.at(property_requests_[cookie.sequence].window);
  xcb_atom_t property = property_requests_[cookie.sequence].property;
  auto it = w.properties_.find(property);

  // The caller is expected to call free() on the return value, so we must use
  // malloc() here.
  xcb_get_property_reply_t* reply = static_cast<xcb_get_property_reply_t*>(
      malloc(sizeof(xcb_get_property_reply_t)));
  EXPECT_TRUE(reply);
  if (it == w.properties_.end()) {
    reply->format = 0;
    reply->sequence = 0;
    reply->type = 0;
    reply->length = 0;
  } else {
    reply->format = it->second.format;
    reply->sequence = cookie.sequence;
    reply->type = it->second.type;
    reply->length = it->second.data.size();
  }
  return reply;
}

void* FakeXcbShim::get_property_value(const xcb_get_property_reply_t* r) {
  if (!r->sequence) {
    // Property was not found. This isn't an error case, just return null.
    return nullptr;
  }
  const FakeProperty& property =
      windows_.at(property_requests_[r->sequence].window)
          .properties_.at(property_requests_[r->sequence].property);
  void* buffer = malloc(property.data.size());
  EXPECT_TRUE(buffer);
  memcpy(buffer, property.data.data(), property.data.size());
  return buffer;
}

int FakeXcbShim::get_property_value_length(const xcb_get_property_reply_t* r) {
  return r->length;
}
