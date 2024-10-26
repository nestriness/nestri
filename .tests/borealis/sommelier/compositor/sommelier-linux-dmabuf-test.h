// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_LINUX_DMABUF_TEST_H_
#define VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_LINUX_DMABUF_TEST_H_

#include <wayland-client.h>

#include "linux-dmabuf-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)

struct linux_dmabuf_test_fixture {
  struct zwp_linux_dmabuf_v1* proxy;
  void* user_data;
  wl_callback* bind_callback_proxy;
};

struct linux_dmabuf_test_fixture get_linux_dmabuf_test_fixture();

#endif  // VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_LINUX_DMABUF_TEST_H_
