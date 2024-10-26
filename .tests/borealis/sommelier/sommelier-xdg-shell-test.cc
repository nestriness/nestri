// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "testing/sommelier-test-util.h"
#include "testing/wayland-test-base.h"

#include "mock-xdg-shell-shim.h"  // NOLINT(build/include_directory)
#include "sommelier.h"            // NOLINT(build/include_directory)
#include "sommelier-ctx.h"        // NOLINT(build/include_directory)
#include "sommelier-xdg-shell.h"  // NOLINT(build/include_directory)
#include "xdg-shell-shim.h"       // NOLINT(build/include_directory)

// Fake constant serial number to be used in the tests below.
const uint32_t kFakeSerial = 721077;

namespace vm_tools {
namespace sommelier {

using ::testing::NiceMock;

class XdgShellTest : public WaylandTestBase {
 public:
  void Connect() override {
    // Setup all the shims.
    set_xdg_positioner_shim(&mock_xdg_positioner_shim_);
    set_xdg_popup_shim(&mock_xdg_popup_shim_);
    set_xdg_toplevel_shim(&mock_xdg_toplevel_shim_);
    set_xdg_surface_shim(&mock_xdg_surface_shim_);
    set_xdg_wm_base_shim(&mock_xdg_wm_base_shim_);

    WaylandTestBase::Connect();

    ctx.use_direct_scale = false;
    client = std::make_unique<FakeWaylandClient>(&ctx);
    ctx.client = client->client;
  }

  void TearDown() override {
    client->Flush();
    WaylandTestBase::TearDown();
  }

 protected:
  NiceMock<MockXdgWmBaseShim> mock_xdg_wm_base_shim_;
  NiceMock<MockXdgPositionerShim> mock_xdg_positioner_shim_;
  NiceMock<MockXdgPopupShim> mock_xdg_popup_shim_;
  NiceMock<MockXdgToplevelShim> mock_xdg_toplevel_shim_;
  NiceMock<MockXdgSurfaceShim> mock_xdg_surface_shim_;

  std::unique_ptr<FakeWaylandClient> client;
};

class XdgWmBaseTest : public XdgShellTest {
 public:
  void SetUp() override {
    EXPECT_CALL(mock_xdg_wm_base_shim_, add_listener(_, _, _))
        .WillOnce([this](struct xdg_wm_base* xdg_wm_base,
                         const xdg_wm_base_listener* listener,
                         void* user_data) {
          sommelier_xdg_wm_base =
              static_cast<struct sl_host_xdg_shell*>(user_data);
          xdg_wm_base_add_listener(xdg_wm_base, listener, user_data);
          return 0;
        });
    WaylandTestBase::SetUp();

    client_surface = client->CreateSurface();
    Pump();
  }

 protected:
  // Sommelier's xdg_wm_base instance.
  struct sl_host_xdg_shell* sommelier_xdg_wm_base = nullptr;
  wl_surface* client_surface = nullptr;
};

TEST_F(XdgWmBaseTest, CreatePositioner_ForwardsCorrectly) {
  EXPECT_CALL(mock_xdg_wm_base_shim_,
              create_positioner(sommelier_xdg_wm_base->proxy));
  xdg_wm_base_create_positioner(client->GetXdgWmBase());
}

TEST_F(XdgWmBaseTest, GetXdgSurface_ForwardsCorrectly) {
  EXPECT_CALL(mock_xdg_wm_base_shim_,
              get_xdg_surface(sommelier_xdg_wm_base->proxy, _));
  xdg_wm_base_get_xdg_surface(client->GetXdgWmBase(), client_surface);
}

TEST_F(XdgWmBaseTest, Ping_SendsCorrectly) {
  EXPECT_CALL(mock_xdg_wm_base_shim_,
              get_user_data(sommelier_xdg_wm_base->proxy))
      .WillOnce([](struct xdg_wm_base* xdg_wm_base) {
        return xdg_wm_base_get_user_data(xdg_wm_base);
      });
  EXPECT_CALL(mock_xdg_wm_base_shim_, send_ping(_, kFakeSerial));

  HostEventHandler(sommelier_xdg_wm_base->proxy)
      ->ping(nullptr, sommelier_xdg_wm_base->proxy, kFakeSerial);
}

TEST_F(XdgWmBaseTest, Pong_ForwardsCorrectly) {
  EXPECT_CALL(mock_xdg_wm_base_shim_, pong(_, kFakeSerial));
  xdg_wm_base_pong(client->GetXdgWmBase(), kFakeSerial);
}

class XdgPositionerTest : public XdgShellTest {
 public:
  void SetUp() override {
    WaylandTestBase::SetUp();

    EXPECT_CALL(mock_xdg_wm_base_shim_, create_positioner(_))
        .WillOnce([](struct xdg_wm_base* xdg_wm_base) {
          return xdg_wm_base_create_positioner(xdg_wm_base);
        });

    EXPECT_CALL(mock_xdg_positioner_shim_, set_user_data(_, _))
        .WillOnce(
            [this](struct xdg_positioner* xdg_positioner, void* user_data) {
              // Capture the object pointers so we can verify below.
              sommelier_positioner = xdg_positioner;
              xdg_positioner_set_user_data(xdg_positioner, user_data);
            });

    client_positioner = client->CreatePositioner();

    Pump();
  }

 protected:
  struct xdg_positioner* sommelier_positioner = nullptr;
  struct xdg_positioner* client_positioner = nullptr;
};

TEST_F(XdgPositionerTest, SetSize_ForwardsUnscaled) {
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_size(sommelier_positioner, 100, 100));
  xdg_positioner_set_size(client_positioner, 100, 100);
}

TEST_F(XdgPositionerTest, SetSize_AppliesCtxScale) {
  ctx.scale = 2.0;
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_size(sommelier_positioner, 50, 50));

  xdg_positioner_set_size(client_positioner, 100, 100);
}

TEST_F(XdgPositionerTest, SetSize_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_size(sommelier_positioner, 100, 100));

  xdg_positioner_set_size(client_positioner, 100, 100);
}

TEST_F(XdgPositionerTest, SetSize_AppliesXdgScaleWithDirectScale) {
  ctx.use_direct_scale = true;
  ctx.xdg_scale_x = 2.0;
  ctx.xdg_scale_y = 4.0;
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_size(sommelier_positioner, 50, 25));

  xdg_positioner_set_size(client_positioner, 100, 100);
}

TEST_F(XdgPositionerTest, SetAnchorRect_ForwardsUnscaled) {
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_anchor_rect(sommelier_positioner, 0, 0, 100, 100));

  xdg_positioner_set_anchor_rect(client_positioner, 0, 0, 100, 100);
}

TEST_F(XdgPositionerTest, SetAnchorRect_AppliesCtxScale) {
  ctx.scale = 2.0;
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_anchor_rect(sommelier_positioner, 0, 0, 50, 50));

  xdg_positioner_set_anchor_rect(client_positioner, 0, 0, 100, 100);
}

TEST_F(XdgPositionerTest, SetAnchorRect_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_anchor_rect(sommelier_positioner, 0, 0, 100, 100));

  xdg_positioner_set_anchor_rect(client_positioner, 0, 0, 100, 100);
}

TEST_F(XdgPositionerTest, SetAnchorRect_AppliesXdgScaleWithDirectScale) {
  ctx.use_direct_scale = true;
  ctx.xdg_scale_x = 2.0;
  ctx.xdg_scale_y = 4.0;

  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_anchor_rect(sommelier_positioner, 0, 0, 50, 25));

  xdg_positioner_set_anchor_rect(client_positioner, 0, 0, 100, 100);
}

TEST_F(XdgPositionerTest, SetOffset_ForwardsUnscaled) {
  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_offset(sommelier_positioner, 100, 100));

  xdg_positioner_set_offset(client_positioner, 100, 100);
}

TEST_F(XdgPositionerTest, SetOffset_AppliesCtxScale) {
  ctx.scale = 2.0;

  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_offset(sommelier_positioner, 50, 50));

  xdg_positioner_set_offset(client_positioner, 100, 100);
}

TEST_F(XdgPositionerTest, SetOffset_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;

  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_offset(sommelier_positioner, 100, 100));

  xdg_positioner_set_offset(client_positioner, 100, 100);
}

TEST_F(XdgPositionerTest, SetOffset_AppliesXdgScaleWithDirectScale) {
  ctx.use_direct_scale = true;
  ctx.xdg_scale_x = 2.0;
  ctx.xdg_scale_y = 4.0;

  EXPECT_CALL(mock_xdg_positioner_shim_,
              set_offset(sommelier_positioner, 50, 25));

  xdg_positioner_set_offset(client_positioner, 100, 100);
}

class XdgSurfaceTest : public XdgShellTest {
 public:
  void SetUp() override {
    EXPECT_CALL(mock_xdg_wm_base_shim_, get_xdg_surface(_, _))
        .WillOnce([](struct xdg_wm_base* xdg_wm_base, wl_surface* wl_surface) {
          return xdg_wm_base_get_xdg_surface(xdg_wm_base, wl_surface);
        });
    EXPECT_CALL(mock_xdg_surface_shim_, add_listener(_, _, _))
        .WillOnce([this](struct xdg_surface* xdg_surface,
                         const xdg_surface_listener* listener,
                         void* user_data) {
          sommelier_xdg_surface =
              static_cast<struct sl_host_xdg_surface*>(user_data);
          xdg_surface_add_listener(xdg_surface, listener, user_data);
          return 0;
        });

    WaylandTestBase::SetUp();

    client_xdg_surface = client->CreateXdgSurface();
    Pump();
  }

 protected:
  struct sl_host_xdg_surface* sommelier_xdg_surface = nullptr;
  struct xdg_surface* client_xdg_surface = nullptr;
};

TEST_F(XdgSurfaceTest, GetToplevel_CreatesToplevel) {
  EXPECT_CALL(mock_xdg_surface_shim_,
              get_toplevel(sommelier_xdg_surface->proxy));

  xdg_surface_get_toplevel(client_xdg_surface);
}

TEST_F(XdgSurfaceTest, GetPopup_CreatesPopup) {
  struct sl_host_xdg_positioner* sommelier_positioner = nullptr;

  EXPECT_CALL(mock_xdg_wm_base_shim_, create_positioner(_))
      .WillOnce([](struct xdg_wm_base* xdg_wm_base) {
        return xdg_wm_base_create_positioner(xdg_wm_base);
      });

  // Capture the positioner object for verification below.
  EXPECT_CALL(mock_xdg_positioner_shim_, set_user_data(_, _))
      .WillOnce([&sommelier_positioner](struct xdg_positioner* xdg_positioner,
                                        void* user_data) {
        sommelier_positioner =
            static_cast<struct sl_host_xdg_positioner*>(user_data);
        xdg_positioner_set_user_data(xdg_positioner, user_data);
      });

  struct xdg_positioner* positioner = client->CreatePositioner();
  Pump();

  EXPECT_CALL(mock_xdg_surface_shim_, get_popup(sommelier_xdg_surface->proxy,
                                                sommelier_xdg_surface->proxy,
                                                sommelier_positioner->proxy));

  xdg_surface_get_popup(client_xdg_surface, client_xdg_surface, positioner);
}

TEST_F(XdgSurfaceTest, SetWindowGeometry_ForwardsUnscaled) {
  EXPECT_CALL(
      mock_xdg_surface_shim_,
      set_window_geometry(sommelier_xdg_surface->proxy, 100, 100, 100, 100));
  xdg_surface_set_window_geometry(client_xdg_surface, 100, 100, 100, 100);
}

TEST_F(XdgSurfaceTest, SetWindowGeometry_AppliesCtxScale) {
  ctx.scale = 2.0;
  EXPECT_CALL(
      mock_xdg_surface_shim_,
      set_window_geometry(sommelier_xdg_surface->proxy, 50, 50, 50, 50));
  xdg_surface_set_window_geometry(client_xdg_surface, 100, 100, 100, 100);
}

TEST_F(XdgSurfaceTest, SetWindowGeometry_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  EXPECT_CALL(
      mock_xdg_surface_shim_,
      set_window_geometry(sommelier_xdg_surface->proxy, 100, 100, 100, 100));
  xdg_surface_set_window_geometry(client_xdg_surface, 100, 100, 100, 100);
}

TEST_F(XdgSurfaceTest, SetWindowGeometry_AppliesXdgScaleWithDirectScale) {
  ctx.use_direct_scale = true;
  ctx.xdg_scale_x = 2.0;
  ctx.xdg_scale_y = 4.0;
  EXPECT_CALL(
      mock_xdg_surface_shim_,
      set_window_geometry(sommelier_xdg_surface->proxy, 50, 25, 50, 25));
  xdg_surface_set_window_geometry(client_xdg_surface, 100, 100, 100, 100);
}

TEST_F(XdgSurfaceTest, AckConfigure_ForwardsCorrectly) {
  EXPECT_CALL(mock_xdg_surface_shim_, ack_configure(_, kFakeSerial));
  xdg_surface_ack_configure(client_xdg_surface, kFakeSerial);
}

TEST_F(XdgSurfaceTest, Configure_SendsCorrectly) {
  EXPECT_CALL(mock_xdg_surface_shim_,
              get_user_data(sommelier_xdg_surface->proxy))
      .WillOnce([](struct xdg_surface* xdg_surface) {
        return xdg_surface_get_user_data(xdg_surface);
      });
  EXPECT_CALL(mock_xdg_surface_shim_, send_configure(_, kFakeSerial));

  HostEventHandler(sommelier_xdg_surface->proxy)
      ->configure(nullptr, sommelier_xdg_surface->proxy, kFakeSerial);
}

}  // namespace sommelier
}  // namespace vm_tools
