// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_XCB_MOCK_XCB_SHIM_H_
#define VM_TOOLS_SOMMELIER_XCB_MOCK_XCB_SHIM_H_

#include <gmock/gmock.h>

#include "fake-xcb-shim.h"  // NOLINT(build/include_directory)
#include "xcb-shim.h"       // NOLINT(build/include_directory)

class MockXcbShim : public XcbShim {
 public:
  MOCK_METHOD(xcb_connection_t*,
              connect,
              (const char* displayname, int* screenp),
              (override));

  MOCK_METHOD(uint32_t, generate_id, (xcb_connection_t * c), (override));

  MOCK_METHOD(xcb_void_cookie_t,
              create_window,
              (xcb_connection_t * c,
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
               const void* value_list),
              (override));

  MOCK_METHOD(xcb_void_cookie_t,
              reparent_window,
              (xcb_connection_t * c,
               xcb_window_t window,
               xcb_window_t parent,
               int16_t x,
               int16_t y),
              (override));

  MOCK_METHOD(xcb_void_cookie_t,
              map_window,
              (xcb_connection_t * c, xcb_window_t window),
              (override));

  MOCK_METHOD(xcb_void_cookie_t,
              configure_window,
              (xcb_connection_t * c,
               xcb_window_t window,
               uint16_t value_mask,
               const void* value_list),
              (override));

  MOCK_METHOD(xcb_void_cookie_t,
              change_property,
              (xcb_connection_t * c,
               uint8_t mode,
               xcb_window_t window,
               xcb_atom_t property,
               xcb_atom_t type,
               uint8_t format,
               uint32_t data_len,
               const void* data),
              (override));

  MOCK_METHOD(xcb_void_cookie_t,
              send_event,
              (xcb_connection_t * c,
               uint8_t propagate,
               xcb_window_t destination,
               uint32_t event_mask,
               const char* event),
              (override));

  MOCK_METHOD(xcb_void_cookie_t,
              change_window_attributes,
              (xcb_connection_t * c,
               xcb_window_t window,
               uint32_t value_mask,
               const void* value_list),
              (override));

  MOCK_METHOD(xcb_get_geometry_cookie_t,
              get_geometry,
              (xcb_connection_t * c, xcb_drawable_t drawable),
              (override));

  MOCK_METHOD(xcb_get_geometry_reply_t*,
              get_geometry_reply,
              (xcb_connection_t * c,
               xcb_get_geometry_cookie_t cookie,
               xcb_generic_error_t** e),
              (override));

  MOCK_METHOD(xcb_get_property_cookie_t,
              get_property,
              (xcb_connection_t * c,
               uint8_t _delete,
               xcb_window_t window,
               xcb_atom_t property,
               xcb_atom_t type,
               uint32_t long_offset,
               uint32_t long_length),
              (override));

  MOCK_METHOD(xcb_get_property_reply_t*,
              get_property_reply,
              (xcb_connection_t * c,
               xcb_get_property_cookie_t cookie,
               xcb_generic_error_t** e),
              (override));

  MOCK_METHOD(void*,
              get_property_value,
              (const xcb_get_property_reply_t* r),
              (override));

  MOCK_METHOD(int,
              get_property_value_length,
              (const xcb_get_property_reply_t* r),
              (override));

  // It's best to centralize ID generation in the fake, even for test cases
  // that never use the fake for anything else. This prevents ID collisions.
  void DelegateIdGenerationToFake() {
    ON_CALL(*this, generate_id).WillByDefault([this](xcb_connection_t* c) {
      return fake_.generate_id(c);
    });
  }

  // Some interactions with XCB, such as getting properties, are too complex to
  // mock. In this case we can delegate to a fake to get semi-realistic
  // behaviour.
  //
  // TODO(cpelling): Build a complete X11 fake instead.
  void DelegateToFake() {
    ON_CALL(*this, generate_id).WillByDefault([this](xcb_connection_t* c) {
      return fake_.generate_id(c);
    });
    ON_CALL(*this, create_window)
        .WillByDefault([this](xcb_connection_t* c, uint8_t depth,
                              xcb_window_t wid, xcb_window_t parent, int16_t x,
                              int16_t y, uint16_t width, uint16_t height,
                              uint16_t border_width, uint16_t _class,
                              xcb_visualid_t visual, uint32_t value_mask,
                              const void* value_list) {
          return fake_.create_window(c, depth, wid, parent, x, y, width, height,
                                     border_width, _class, visual, value_mask,
                                     value_list);
        });
    ON_CALL(*this, reparent_window)
        .WillByDefault([this](xcb_connection_t* c, xcb_window_t window,
                              xcb_window_t parent, int16_t x, int16_t y) {
          return fake_.reparent_window(c, window, parent, x, y);
        });
    ON_CALL(*this, map_window)
        .WillByDefault([this](xcb_connection_t* c, xcb_window_t window) {
          return fake_.map_window(c, window);
        });
    ON_CALL(*this, change_property)
        .WillByDefault([this](xcb_connection_t* c, uint8_t mode,
                              xcb_window_t window, xcb_atom_t property,
                              xcb_atom_t type, uint8_t format,
                              uint32_t data_len, const void* data) {
          return fake_.change_property(c, mode, window, property, type, format,
                                       data_len, data);
        });
    ON_CALL(*this, get_property)
        .WillByDefault([this](xcb_connection_t* c, uint8_t _delete,
                              xcb_window_t window, xcb_atom_t property,
                              xcb_atom_t type, uint32_t long_offset,
                              uint32_t long_length) {
          return fake_.get_property(c, _delete, window, property, type,
                                    long_offset, long_length);
        });
    ON_CALL(*this, get_property_reply)
        .WillByDefault([this](xcb_connection_t* c,
                              xcb_get_property_cookie_t cookie,
                              xcb_generic_error_t** e) {
          return fake_.get_property_reply(c, cookie, e);
        });
    ON_CALL(*this, get_property_value)
        .WillByDefault([this](const xcb_get_property_reply_t* r) {
          return fake_.get_property_value(r);
        });
    ON_CALL(*this, get_property_value_length)
        .WillByDefault([this](const xcb_get_property_reply_t* r) {
          return fake_.get_property_value_length(r);
        });
  }

 private:
  FakeXcbShim fake_;
};

#endif  // VM_TOOLS_SOMMELIER_XCB_MOCK_XCB_SHIM_H_
