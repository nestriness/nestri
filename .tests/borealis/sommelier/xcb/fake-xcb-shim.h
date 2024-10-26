// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_XCB_FAKE_XCB_SHIM_H_
#define VM_TOOLS_SOMMELIER_XCB_FAKE_XCB_SHIM_H_

#include <unordered_map>
#include <vector>

#include "xcb-shim.h"  // NOLINT(build/include_directory)

class FakeProperty {
 public:
  xcb_atom_t type;
  uint8_t format;
  std::vector<unsigned char> data;
};

class FakeWindow {
 public:
  uint8_t depth;
  xcb_window_t wid;
  xcb_window_t parent;
  int16_t x;
  int16_t y;
  uint16_t width;
  uint16_t height;
  uint16_t border_width;
  uint16_t _class;
  bool mapped = false;
  xcb_visualid_t visual;

  std::unordered_map<xcb_atom_t, FakeProperty> properties_;
};

// Metadata of a `xcb_get_property` request, stored so we can look up
// the requested information for get_property_reply, get_property_value,
// get_property_value_length.
class PropertyRequestData {
 public:
  xcb_window_t window;
  xcb_atom_t property;
};

// Partial fake of the XCB API.
//
// Supports some very basic window management and property getting/setting.
// Some methods are unimplemented and will assert if called.
class FakeXcbShim : public XcbShim {
 public:
  xcb_connection_t* connect(const char* displayname, int* screenp) override;
  uint32_t generate_id(xcb_connection_t* c) override;
  xcb_void_cookie_t create_window(xcb_connection_t* c,
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
                                  const void* value_list) override;
  xcb_void_cookie_t reparent_window(xcb_connection_t* c,
                                    xcb_window_t window,
                                    xcb_window_t parent,
                                    int16_t x,
                                    int16_t y) override;
  xcb_void_cookie_t map_window(xcb_connection_t* c,
                               xcb_window_t window) override;
  xcb_void_cookie_t configure_window(xcb_connection_t* c,
                                     xcb_window_t window,
                                     uint16_t value_mask,
                                     const void* value_list) override;
  xcb_void_cookie_t change_property(xcb_connection_t* c,
                                    uint8_t mode,
                                    xcb_window_t window,
                                    xcb_atom_t property,
                                    xcb_atom_t type,
                                    uint8_t format,
                                    uint32_t data_len,
                                    const void* data) override;
  xcb_void_cookie_t send_event(xcb_connection_t* c,
                               uint8_t propagate,
                               xcb_window_t destination,
                               uint32_t event_mask,
                               const char* event) override;
  xcb_void_cookie_t change_window_attributes(xcb_connection_t* c,
                                             xcb_window_t window,
                                             uint32_t value_mask,
                                             const void* value_list) override;
  xcb_get_geometry_cookie_t get_geometry(xcb_connection_t* c,
                                         xcb_drawable_t drawable) override;
  xcb_get_geometry_reply_t* get_geometry_reply(
      xcb_connection_t* c,
      xcb_get_geometry_cookie_t cookie,
      xcb_generic_error_t** e) override;
  xcb_get_property_cookie_t get_property(xcb_connection_t* c,
                                         uint8_t _delete,
                                         xcb_window_t window,
                                         xcb_atom_t property,
                                         xcb_atom_t type,
                                         uint32_t long_offset,
                                         uint32_t long_length) override;
  xcb_get_property_reply_t* get_property_reply(
      xcb_connection_t* c,
      xcb_get_property_cookie_t cookie,
      xcb_generic_error_t** e) override;
  void* get_property_value(const xcb_get_property_reply_t* r) override;
  int get_property_value_length(const xcb_get_property_reply_t* r) override;

 private:
  uint32_t next_id_ = 1;

  // Keep track of windows and their properties in the faked X11 environment.
  std::unordered_map<xcb_window_t, FakeWindow> windows_;

  // State tracking for X11 property requests. Each call to `get_property`
  // returns a xcb_get_property_cookie_t, whose `sequence` member is set to an
  // incrementing ID number. The metadata of the property request is stored in
  // this map associated to that ID, so we can retrieve it later.
  std::unordered_map<unsigned int, PropertyRequestData> property_requests_;

  // ID to assign to the next xcb_get_property_cookie_t we allocate.
  unsigned int next_cookie_ = 1;
};

#endif  // VM_TOOLS_SOMMELIER_XCB_FAKE_XCB_SHIM_H_
