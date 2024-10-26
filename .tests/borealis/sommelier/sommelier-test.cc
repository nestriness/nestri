// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/socket.h>

#include "sommelier.h"                  // NOLINT(build/include_directory)
#include "sommelier-util.h"             // NOLINT(build/include_directory)
#include "testing/wayland-test-base.h"  // NOLINT(build/include_directory)

namespace vm_tools {
namespace sommelier {

using WaylandTest = WaylandTestBase;

TEST_F(WaylandTest, CanCommitToEmptySurface) {
  wl_surface* surface = wl_compositor_create_surface(ctx.compositor->internal);
  wl_surface_commit(surface);
}

}  // namespace sommelier
}  // namespace vm_tools
