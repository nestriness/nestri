// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xcb/xproto.h>

#include "testing/x11-test-base.h"
#include "xcb/fake-xcb-shim.h"

namespace vm_tools {
namespace sommelier {

using X11EventTest = X11TestBase;

TEST_F(X11EventTest, MapRequestCreatesFrameWindow) {
  sl_window* window = CreateWindowWithoutRole();
  xcb_map_request_event_t event;
  event.response_type = XCB_MAP_REQUEST;
  event.window = window->id;
  EXPECT_EQ(window->frame_id, XCB_WINDOW_NONE);

  EXPECT_CALL(xcb, generate_id).WillOnce(testing::Return(456));
  sl_handle_map_request(&ctx, &event);

  EXPECT_EQ(window->frame_id, 456u);
}

TEST_F(X11EventTest, MapRequestIssuesMapWindow) {
  sl_window* window = CreateWindowWithoutRole();
  xcb_map_request_event_t event;
  event.response_type = XCB_MAP_REQUEST;
  event.window = window->id;

  EXPECT_CALL(xcb, generate_id).WillOnce(testing::Return(456));
  EXPECT_CALL(xcb, map_window(testing::_, window->id)).Times(1);
  EXPECT_CALL(xcb, map_window(testing::_, 456u)).Times(1);

  sl_handle_map_request(&ctx, &event);
}

TEST_F(X11EventTest, MapRequestGetsWmName) {
  std::string windowName("Fred");
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, windowName.size(),
                      windowName.c_str());
  EXPECT_EQ(window->name, nullptr);

  xcb_map_request_event_t event;
  event.response_type = XCB_MAP_REQUEST;
  event.window = window->id;
  sl_handle_map_request(&ctx, &event);

  EXPECT_EQ(window->name, windowName);
}

TEST_F(X11EventTest, ListensToWmNameChanges) {
  std::string windowName("Fred");
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, windowName.size(),
                      windowName.c_str());

  xcb_property_notify_event_t event;
  event.response_type = XCB_PROPERTY_NOTIFY;
  event.window = window->id;
  event.atom = XCB_ATOM_WM_NAME;
  event.state = XCB_PROPERTY_NEW_VALUE;
  sl_handle_property_notify(&ctx, &event);

  EXPECT_EQ(window->name, windowName);
}

TEST_F(X11EventTest, NetWmNameOverridesWmname) {
  std::string boringWindowName("Fred");
  std::string fancyWindowName("I ♥️ Unicode 🦄🌈");
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                      boringWindowName.size(), boringWindowName.c_str());
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      ctx.atoms[ATOM_NET_WM_NAME].value, XCB_ATOM_STRING, 8,
                      fancyWindowName.size(), fancyWindowName.c_str());

  xcb_property_notify_event_t event;
  event.response_type = XCB_PROPERTY_NOTIFY;
  event.window = window->id;
  event.atom = XCB_ATOM_WM_NAME;
  event.state = XCB_PROPERTY_NEW_VALUE;
  sl_handle_property_notify(&ctx, &event);

  event.atom = ctx.atoms[ATOM_NET_WM_NAME].value;
  sl_handle_property_notify(&ctx, &event);

  EXPECT_EQ(window->name, fancyWindowName);
}

TEST_F(X11EventTest, MapRequestStoresSteamGameId) {
  uint32_t steam_game_id = 123456;
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      ctx.atoms[ATOM_STEAM_GAME].value, XCB_ATOM_CARDINAL, 32,
                      1, &steam_game_id);

  xcb_map_request_event_t event;
  event.response_type = XCB_MAP_REQUEST;
  event.window = window->id;
  sl_handle_map_request(&ctx, &event);

  EXPECT_EQ(window->steam_game_id, steam_game_id);
}

TEST_F(X11EventTest, PropertyNotifyStoresSteamId) {
  uint32_t steam_game_id = 123456;
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      ctx.atoms[ATOM_STEAM_GAME].value, XCB_ATOM_CARDINAL, 32,
                      1, &steam_game_id);

  xcb_property_notify_event_t event;
  event.response_type = XCB_PROPERTY_NOTIFY;
  event.window = window->id;
  event.atom = ctx.atoms[ATOM_STEAM_GAME].value;
  event.state = XCB_PROPERTY_NEW_VALUE;
  sl_handle_property_notify(&ctx, &event);

  EXPECT_EQ(window->steam_game_id, steam_game_id);
}

TEST_F(X11EventTest, MapRequestParsesSteamIdFromClass) {
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  const char clazz[] = "steam_app_7890\0steam_app_7890";
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sizeof(clazz),
                      clazz);

  xcb_map_request_event_t event;
  event.response_type = XCB_MAP_REQUEST;
  event.window = window->id;
  sl_handle_map_request(&ctx, &event);

  EXPECT_EQ(window->steam_game_id, 7890);
}

TEST_F(X11EventTest, MapRequestPrefersSteamGameIdOverClass) {
  uint32_t steam_game_id = 123456;
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  const char clazz[] = "steam_app_7890\0steam_app_7890";
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sizeof(clazz),
                      clazz);
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      ctx.atoms[ATOM_STEAM_GAME].value, XCB_ATOM_CARDINAL, 32,
                      1, &steam_game_id);

  // Act: map the window
  xcb_map_request_event_t event;
  event.response_type = XCB_MAP_REQUEST;
  event.window = window->id;
  sl_handle_map_request(&ctx, &event);

  // Assert: Uses the ID from the STEAM_GAME property, not from WM_CLASS
  EXPECT_EQ(window->steam_game_id, steam_game_id);
}

TEST_F(X11EventTest, PropertyNotifyParsesSteamIdFromClassAsFallback) {
  uint32_t steam_game_id = 123456;
  xcb.DelegateToFake();
  sl_window* window = CreateWindowWithoutRole();
  xcb.create_window(nullptr, 32, window->id, XCB_WINDOW_NONE, 0, 0, 800, 600, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);

  // Act: set STEAM_GAME to one ID, and WM_CLASS to specify another
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      ctx.atoms[ATOM_STEAM_GAME].value, XCB_ATOM_CARDINAL, 32,
                      1, &steam_game_id);
  {
    xcb_property_notify_event_t event;
    event.response_type = XCB_PROPERTY_NOTIFY;
    event.window = window->id;
    event.atom = ctx.atoms[ATOM_STEAM_GAME].value;
    event.state = XCB_PROPERTY_NEW_VALUE;
    sl_handle_property_notify(&ctx, &event);
  }

  const char clazz[] = "steam_app_7890\0steam_app_7890";
  xcb.change_property(nullptr, XCB_PROP_MODE_REPLACE, window->id,
                      XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sizeof(clazz),
                      clazz);
  {
    xcb_property_notify_event_t event;
    event.response_type = XCB_PROPERTY_NOTIFY;
    event.window = window->id;
    event.atom = XCB_ATOM_WM_CLASS;
    event.state = XCB_PROPERTY_NEW_VALUE;
    sl_handle_property_notify(&ctx, &event);
  }

  // Assert: STEAM_GAME is preferred, even though WM_CLASS was changed last.
  EXPECT_EQ(window->steam_game_id, steam_game_id);
}

}  // namespace sommelier
}  // namespace vm_tools
