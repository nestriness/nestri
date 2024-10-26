// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "sommelier-ctx.h"        // NOLINT(build/include_directory)
#include "sommelier-transform.h"  // NOLINT(build/include_directory)
#include "testing/x11-test-base.h"

namespace vm_tools {
namespace sommelier {

using TransformDirectScaleTest = X11DirectScaleTest;

class TransformTest : public ::testing::Test {
 public:
  void SetUp() override {
    sl_context_init_default(&ctx);

    // Test only for stable_scaling since it'll be enabled on by
    // default unless there's an issue.
    ctx.stable_scaling = true;

    // Reset any object variables that are used to a reasonable default.
    sl_transform_reset_surface_scale(&ctx, &fake_surface);
  }

 protected:
  sl_context ctx;
  sl_host_surface fake_surface;
};

TEST_F(TransformTest, TransformViewportScale_UnscaledWithoutDirectScale) {
  ctx.scale = 1;
  int width = 16;
  int height = 16;

  sl_transform_viewport_scale(&ctx, &fake_surface, 1, &width, &height);
  EXPECT_EQ(width, 16);
  EXPECT_EQ(height, 16);
}

TEST_F(TransformTest, TransformViewportScale_ScaledWithoutDirectScale) {
  ctx.scale = 2;
  int width = 16;
  int height = 16;

  sl_transform_viewport_scale(&ctx, &fake_surface, 1, &width, &height);
  EXPECT_EQ(width, 8);
  EXPECT_EQ(height, 8);
}

TEST_F(TransformTest, TransformViewportScale_ContextScaledWithoutDirectScale) {
  int width = 16;
  int height = 16;

  sl_transform_viewport_scale(&ctx, &fake_surface, 3, &width, &height);
  EXPECT_EQ(width, 6);
  EXPECT_EQ(height, 6);
}

TEST_F(TransformTest, TransformViewportScale_DoubleScaledWithoutDirectScale) {
  ctx.scale = 0.9;

  int width = 16;
  int height = 16;

  sl_transform_viewport_scale(&ctx, &fake_surface, 1.6, &width, &height);
  EXPECT_EQ(width, 12);
  EXPECT_EQ(height, 12);
}

TEST_F(TransformTest, TransformViewportScale_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.xdg_scale_x = 1;
  fake_surface.xdg_scale_y = 1;

  int width = 16;
  int height = 16;

  sl_transform_viewport_scale(&ctx, &fake_surface, 1, &width, &height);
  EXPECT_EQ(width, 16);
  EXPECT_EQ(height, 16);
}

TEST_F(TransformTest, TransformViewportScale_SurfaceScaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 2;
  fake_surface.xdg_scale_y = 4;

  int width = 16;
  int height = 16;

  sl_transform_viewport_scale(&ctx, &fake_surface, 1, &width, &height);
  EXPECT_EQ(width, 8);
  EXPECT_EQ(height, 4);
}

TEST_F(TransformTest, TransformDamageCoord_UnscaledWithoutDirectScale) {
  ctx.scale = 1;

  int64_t x1 = 16;
  int64_t x2 = 16;
  int64_t y1 = 16;
  int64_t y2 = 16;

  sl_transform_damage_coord(&ctx, &fake_surface, 1, 1, &x1, &y1, &x2, &y2);

  EXPECT_EQ(x1, 15);
  EXPECT_EQ(y1, 15);
  EXPECT_EQ(x2, 17);
  EXPECT_EQ(y2, 17);
}

TEST_F(TransformTest, TransformDamageCoord_ScaledWithoutDirectScale) {
  ctx.scale = 1;
  int64_t x1 = 16;
  int64_t x2 = 16;
  int64_t y1 = 16;
  int64_t y2 = 16;

  sl_transform_damage_coord(&ctx, &fake_surface, 1.6, 1.6, &x1, &y1, &x2, &y2);

  EXPECT_EQ(x1, 9);
  EXPECT_EQ(y1, 9);
  EXPECT_EQ(x2, 11);
  EXPECT_EQ(y2, 11);
}

TEST_F(TransformTest, TransformDamageCoord_ContextScaledWithoutDirectScale) {
  ctx.scale = 0.9;

  int64_t x1 = 16;
  int64_t x2 = 16;
  int64_t y1 = 16;
  int64_t y2 = 16;

  sl_transform_damage_coord(&ctx, &fake_surface, 1, 1, &x1, &y1, &x2, &y2);

  EXPECT_EQ(x1, 16);
  EXPECT_EQ(y1, 16);
  EXPECT_EQ(x2, 19);
  EXPECT_EQ(y2, 19);
}

TEST_F(TransformTest, TransformDamageCoord_DoubleScaledWithoutDirectScale) {
  ctx.scale = 0.9;

  int64_t x1 = 16;
  int64_t x2 = 16;
  int64_t y1 = 16;
  int64_t y2 = 16;

  sl_transform_damage_coord(&ctx, &fake_surface, 1.6, 1.6, &x1, &y1, &x2, &y2);

  EXPECT_EQ(x1, 10);
  EXPECT_EQ(y1, 10);
  EXPECT_EQ(x2, 12);
  EXPECT_EQ(y2, 12);
}

TEST_F(TransformTest, TransformDamageCoord_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 1;
  fake_surface.xdg_scale_y = 1;

  int64_t x1 = 16;
  int64_t x2 = 16;
  int64_t y1 = 16;
  int64_t y2 = 16;

  sl_transform_damage_coord(&ctx, &fake_surface, 1, 1, &x1, &y1, &x2, &y2);

  EXPECT_EQ(x1, 16);
  EXPECT_EQ(y1, 16);
  EXPECT_EQ(x2, 16);
  EXPECT_EQ(y2, 16);
}

TEST_F(TransformTest, TransformDamageCoord_ScaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 2;
  fake_surface.xdg_scale_y = 4;

  int64_t x1 = 16;
  int64_t x2 = 16;
  int64_t y1 = 16;
  int64_t y2 = 16;

  sl_transform_damage_coord(&ctx, &fake_surface, 1.6, 1.6, &x1, &y1, &x2, &y2);

  EXPECT_EQ(x1, 5);
  EXPECT_EQ(y1, 2);
  EXPECT_EQ(x2, 5);
  EXPECT_EQ(y2, 2);
}

TEST_F(TransformTest, HostToGuest_UnscaledWithoutDirectScale) {
  ctx.scale = 1;
  int x = 16;
  int y = 16;

  sl_transform_host_to_guest(&ctx, &fake_surface, &x, &y);
  EXPECT_EQ(x, 16);
  EXPECT_EQ(y, 16);
}

TEST_F(TransformTest, HostToGuest_ScaledWithoutDirectScale) {
  ctx.scale = 0.9;
  int x = 16;
  int y = 16;
  sl_transform_host_to_guest(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, 15);
  EXPECT_EQ(y, 15);
}

TEST_F(TransformTest, HostToGuest_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 1;
  fake_surface.xdg_scale_y = 1;

  int x = 16;
  int y = 16;

  sl_transform_host_to_guest(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, 16);
  EXPECT_EQ(y, 16);
}

TEST_F(TransformTest, HostToGuest_ScaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 0.9;
  fake_surface.xdg_scale_y = 1.6;

  int x = 16;
  int y = 16;

  sl_transform_host_to_guest(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, 15);
  EXPECT_EQ(y, 26);
}

TEST_F(TransformTest, HostToGuestFixed_UnscaledWithoutDirectScale) {
  ctx.scale = 1;
  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(wl_fixed_to_double(x), 16.0);
  EXPECT_EQ(wl_fixed_to_double(y), 16.0);
}

TEST_F(TransformTest, HostToGuestFixed_ScaledWithoutDirectScale) {
  ctx.scale = 0.9;
  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, wl_fixed_from_double(16 * 0.9));
  EXPECT_EQ(y, wl_fixed_from_double(16 * 0.9));
}

TEST_F(TransformTest, HostToGuestFixed_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 1;
  fake_surface.xdg_scale_y = 1;

  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(wl_fixed_to_double(x), 16.0);
  EXPECT_EQ(wl_fixed_to_double(y), 16.0);
}

TEST_F(TransformTest, HostToGuestFixed_ScaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 0.9;
  fake_surface.xdg_scale_y = 1.6;

  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, wl_fixed_from_double(0.9 * 16));
  EXPECT_EQ(y, wl_fixed_from_double(1.6 * 16));
}

TEST_F(TransformTest, HostToGuestFixedCoord_UnscaledWithoutDirectScale) {
  ctx.scale = 1;
  wl_fixed_t coord = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &coord, 0u);

  EXPECT_EQ(wl_fixed_to_double(coord), 16.0);
}

TEST_F(TransformTest, HostToGuestFixedCoord_ScaledWithoutDirectScale) {
  ctx.scale = 0.9;
  wl_fixed_t coord = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &coord, 0u);

  EXPECT_EQ(coord, wl_fixed_from_double(16 * 0.9));
}

TEST_F(TransformTest, HostToGuestFixedCoord_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_y = 1;

  wl_fixed_t coord = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &coord, 0u);

  EXPECT_EQ(wl_fixed_to_double(coord), 16);
}

TEST_F(TransformTest, HostToGuestFixedCoord_ScaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_y = 1.6;

  wl_fixed_t coord = wl_fixed_from_double(16.0);

  sl_transform_host_to_guest_fixed(&ctx, &fake_surface, &coord, 0u);

  EXPECT_EQ(coord, wl_fixed_from_double(1.6 * 16));
}

TEST_F(TransformTest, GuestToHost_UnscaledWithoutDirectScale) {
  ctx.scale = 1;
  int x = 16;
  int y = 16;

  sl_transform_guest_to_host(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, 16);
  EXPECT_EQ(y, 16);
}

TEST_F(TransformTest, GuestToHost_ScaledWithoutDirectScale) {
  ctx.scale = 0.9;
  int x = 15;
  int y = 15;

  sl_transform_guest_to_host(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, 16);
  EXPECT_EQ(y, 16);
}

TEST_F(TransformTest, GuestToHost_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 1;
  fake_surface.xdg_scale_y = 1;

  int x = 16;
  int y = 16;

  sl_transform_guest_to_host(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, 16);
  EXPECT_EQ(y, 16);
}

TEST_F(TransformTest, GuestToHost_ScaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 0.9;
  fake_surface.xdg_scale_y = 1.6;

  int x = 15;
  int y = 26;

  sl_transform_guest_to_host(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, 16);
  EXPECT_EQ(y, 16);
}

TEST_F(TransformTest, GuestToHostFixed_UnscaledWithoutDirectScale) {
  ctx.scale = 1;

  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_guest_to_host_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(wl_fixed_to_double(x), 16.0);
  EXPECT_EQ(wl_fixed_to_double(y), 16.0);
}

TEST_F(TransformTest, GuestToHostFixed_ScaledWithoutDirectScale) {
  ctx.scale = 0.9;

  wl_fixed_t x = wl_fixed_from_double(15.0);
  wl_fixed_t y = wl_fixed_from_double(15.0);

  sl_transform_guest_to_host_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, wl_fixed_from_double(15 / 0.9));
  EXPECT_EQ(y, wl_fixed_from_double(15 / 0.9));
}

TEST_F(TransformTest, GuestToHostFixed_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 1;
  fake_surface.xdg_scale_y = 1;

  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_guest_to_host_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(wl_fixed_to_double(x), 16.0);
  EXPECT_EQ(wl_fixed_to_double(y), 16.0);
}

TEST_F(TransformTest, GuestToHostFixed_ScaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_x = 0.9;
  fake_surface.xdg_scale_y = 1.6;

  wl_fixed_t x = wl_fixed_from_double(15.0);
  wl_fixed_t y = wl_fixed_from_double(26.0);

  sl_transform_guest_to_host_fixed(&ctx, &fake_surface, &x, &y);

  EXPECT_EQ(x, wl_fixed_from_double(15 / 0.9));
  EXPECT_EQ(y, wl_fixed_from_double(26 / 1.6));
}

TEST_F(TransformTest, GuestToHostFixedCoord_UnscaledWithoutDirectScale) {
  ctx.scale = 1;

  wl_fixed_t coord = wl_fixed_from_double(16.0);

  sl_transform_guest_to_host_fixed(&ctx, &fake_surface, &coord, 0u);

  EXPECT_EQ(wl_fixed_to_double(coord), 16.0);
}

TEST_F(TransformTest, GuestToHostFixedCoord_ScaledWithoutDirectScale) {
  ctx.scale = 0.9;

  wl_fixed_t coord = wl_fixed_from_double(15.0);

  sl_transform_guest_to_host_fixed(&ctx, &fake_surface, &coord, 0u);

  EXPECT_EQ(coord, wl_fixed_from_double(15 / 0.9));
}

TEST_F(TransformTest, GuestToHostFixedCoord_UnscaledWithDirectScale) {
  ctx.use_direct_scale = true;
  fake_surface.has_own_scale = true;
  fake_surface.xdg_scale_y = 1;

  wl_fixed_t coord = wl_fixed_from_double(16.0);

  sl_transform_guest_to_host_fixed(&ctx, &fake_surface, &coord, 0u);

  EXPECT_EQ(wl_fixed_to_double(coord), 16.0);
}

TEST_F(TransformDirectScaleTest, Pointer_WithViewportOverride) {
  ctx.viewport_resize = true;
  sl_window* window = CreateToplevelWindow();
  window->viewport_override = true;
  window->viewport_pointer_scale = 1.1;
  sl_host_surface* surface = window->paired_surface;
  surface->has_own_scale = true;
  surface->xdg_scale_x = 0.9;
  surface->xdg_scale_y = 1.6;

  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_pointer(&ctx, surface, &x, &y);

  EXPECT_EQ(x, wl_fixed_from_double(0.9 * 16 * 1.1));
  EXPECT_EQ(y, wl_fixed_from_double(1.6 * 16 * 1.1));
}

TEST_F(TransformDirectScaleTest, Pointer_WithoutViewportOverride) {
  ctx.viewport_resize = true;
  sl_window* window = CreateToplevelWindow();
  window->viewport_override = false;
  window->viewport_pointer_scale = 0.9;
  sl_host_surface* surface = window->paired_surface;
  ctx.use_direct_scale = true;
  surface->has_own_scale = true;
  surface->xdg_scale_x = 0.9;
  surface->xdg_scale_y = 1.6;

  wl_fixed_t x = wl_fixed_from_double(16.0);
  wl_fixed_t y = wl_fixed_from_double(16.0);

  sl_transform_pointer(&ctx, surface, &x, &y);

  EXPECT_EQ(x, wl_fixed_from_double(0.9 * 16));
  EXPECT_EQ(y, wl_fixed_from_double(1.6 * 16));
}
}  // namespace sommelier
}  // namespace vm_tools
