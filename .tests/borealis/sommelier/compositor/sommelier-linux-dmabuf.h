// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_LINUX_DMABUF_H_
#define VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_LINUX_DMABUF_H_

#include <stdint.h>

#include "../sommelier.h"  // NOLINT(build/include_directory)

#define SL_LINUX_DMABUF_MAX_VERSION 4u

struct gbm_device;
bool sl_linux_dmabuf_fixup_plane0_params(gbm_device* gbm,
                                         int32_t fd,
                                         uint32_t* out_stride,
                                         uint32_t* out_modifier_hi,
                                         uint32_t* out_modifier_lo);

struct sl_linux_dmabuf_host_buffer_create_info {
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t format;
  int dmabuf_fd;
  bool is_virtgpu_buffer;
};

// NOLINTNEXTLINE(build/class)
struct sl_host_buffer* sl_linux_dmabuf_create_host_buffer(
    struct sl_context* ctx,  // NOLINT(build/class)
    struct wl_client* client,
    struct wl_buffer* buffer_proxy,
    uint32_t buffer_id,
    const struct sl_linux_dmabuf_host_buffer_create_info* info);

#endif  // VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_LINUX_DMABUF_H_
