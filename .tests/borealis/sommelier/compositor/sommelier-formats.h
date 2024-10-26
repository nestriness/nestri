// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_FORMATS_H_
#define VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_FORMATS_H_

#include <stddef.h>
#include <stdint.h>

bool sl_drm_format_is_supported(uint32_t format);

bool sl_shm_format_is_supported(uint32_t format);

uint32_t sl_shm_format_from_drm_format(uint32_t drm_format);

uint32_t sl_shm_format_to_drm_format(uint32_t shm_format);

size_t sl_shm_format_bpp(uint32_t format);

size_t sl_shm_format_num_planes(uint32_t format);

size_t sl_shm_format_plane_y_subsampling(uint32_t format, size_t plane);

int sl_shm_format_plane_offset(uint32_t format,
                               size_t plane,
                               size_t height,
                               size_t stride);

size_t sl_shm_format_size(uint32_t format, size_t height, size_t stride);

#endif  // VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_FORMATS_H_
