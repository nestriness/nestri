// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../sommelier.h"       // NOLINT(build/include_directory)
#include "sommelier-formats.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <libdrm/drm_fourcc.h>
#include <wayland-client.h>

size_t (*plane_y_subsampling_func)(size_t plane);
int (*plane_offset_func)(size_t plane);

struct sl_format_metadata {
  uint32_t drm_format;
  uint32_t shm_format;

  size_t bpp;
  uint32_t num_planes;

  size_t plane_y_subsampling;
  size_t (*plane_y_subsampling_func)(size_t plane);

  int plane_offset;
  int (*plane_offset_func)(size_t plane, size_t height, size_t stride);
};

static size_t nv12_plane_y_subsampling(size_t plane) {
  assert(plane < 2);
  return plane == 0 ? 1 : 2;
}

static int nv12_plane_offset(size_t plane, size_t height, size_t stride) {
  const size_t offset[] = {0, 1};
  assert(plane < ARRAY_SIZE(offset));
  return offset[plane] * height * stride;
}

#define SINGLE_PLANE_FORMAT(format_suffix, _bpp)                               \
  {                                                                            \
    .drm_format = DRM_FORMAT_##format_suffix,                                  \
    .shm_format = WL_SHM_FORMAT_##format_suffix, .bpp = _bpp, .num_planes = 1, \
    .plane_y_subsampling = 1, .plane_offset = 0,                               \
  }

static struct sl_format_metadata* last_accessed_format = nullptr;
static struct sl_format_metadata format_table[] = {
    {
        .drm_format = DRM_FORMAT_NV12,
        .shm_format = WL_SHM_FORMAT_NV12,
        .bpp = 1,
        .num_planes = 2,
        .plane_y_subsampling_func = nv12_plane_y_subsampling,
        .plane_offset_func = nv12_plane_offset,
    },
    SINGLE_PLANE_FORMAT(RGB565, 2),
    SINGLE_PLANE_FORMAT(XRGB8888, 4),
    SINGLE_PLANE_FORMAT(ARGB8888, 4),
    SINGLE_PLANE_FORMAT(XBGR8888, 4),
    SINGLE_PLANE_FORMAT(ABGR8888, 4),
};

static struct sl_format_metadata* get_metadata_for_format(uint32_t format,
                                                          bool is_drm_format) {
  // format is a wl_shm::format    if is_drm_format == false
  //        is a drm fourcc format otherwise
  uint32_t test_format;

  if (last_accessed_format) {
    test_format = is_drm_format ? last_accessed_format->drm_format
                                : last_accessed_format->shm_format;
    if (format == test_format)
      return last_accessed_format;
  }

  for (auto& meta : format_table) {
    uint32_t test_format = is_drm_format ? meta.drm_format : meta.shm_format;
    if (format == test_format) {
      last_accessed_format = &meta;
      return &meta;
    }
  }

  return nullptr;
}

bool sl_drm_format_is_supported(uint32_t format) {
  return !!get_metadata_for_format(format, /*is_drm_format=*/true);
}

bool sl_shm_format_is_supported(uint32_t format) {
  return !!get_metadata_for_format(format, /*is_drm_format=*/false);
}

uint32_t sl_shm_format_from_drm_format(uint32_t drm_format) {
  struct sl_format_metadata* meta = get_metadata_for_format(drm_format, true);
  assert(meta);
  return meta->shm_format;
}

uint32_t sl_shm_format_to_drm_format(uint32_t shm_format) {
  struct sl_format_metadata* meta = get_metadata_for_format(shm_format, false);
  assert(meta);
  return meta->drm_format;
}

size_t sl_shm_format_bpp(uint32_t format) {
  struct sl_format_metadata* meta = get_metadata_for_format(format, false);
  assert(meta);
  return meta->bpp;
}

size_t sl_shm_format_num_planes(uint32_t format) {
  struct sl_format_metadata* meta = get_metadata_for_format(format, false);
  assert(meta);
  return meta->num_planes;
}

size_t sl_shm_format_plane_y_subsampling(uint32_t format, size_t plane) {
  struct sl_format_metadata* meta = get_metadata_for_format(format, false);
  assert(meta);
  return meta->plane_y_subsampling_func ? meta->plane_y_subsampling_func(plane)
                                        : meta->plane_y_subsampling;
}

int sl_shm_format_plane_offset(uint32_t format,
                               size_t plane,
                               size_t height,
                               size_t stride) {
  struct sl_format_metadata* meta = get_metadata_for_format(format, false);
  assert(meta);
  return meta->plane_offset_func
             ? meta->plane_offset_func(plane, height, stride)
             : meta->plane_offset;
}

static size_t sl_shm_format_plane_size(uint32_t format,
                                       size_t plane,
                                       size_t height,
                                       size_t stride) {
  return height / sl_shm_format_plane_y_subsampling(format, plane) * stride;
}

size_t sl_shm_format_size(uint32_t format, size_t height, size_t stride) {
  size_t i, num_planes = sl_shm_format_num_planes(format);
  size_t total_size = 0;

  for (i = 0; i < num_planes; ++i) {
    size_t size = sl_shm_format_plane_size(format, i, height, stride);
    size_t offset = sl_shm_format_plane_offset(format, i, height, stride);
    total_size = MAX(total_size, size + offset);
  }

  return total_size;
}
