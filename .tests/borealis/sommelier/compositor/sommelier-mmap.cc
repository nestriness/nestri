// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-mmap.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <unistd.h>

#include "../sommelier.h"          // NOLINT(build/include_directory)
#include "../sommelier-tracing.h"  // NOLINT(build/include_directory)

struct sl_mmap* sl_mmap_create(int fd,
                               size_t size,
                               size_t bpp,
                               size_t num_planes,
                               size_t offset0,
                               size_t stride0,
                               size_t offset1,
                               size_t stride1,
                               size_t y_ss0,
                               size_t y_ss1) {
  TRACE_EVENT("shm", "sl_mmap_create");
  struct sl_mmap* map = new sl_mmap();
  map->refcount = 1;
  map->fd = fd;
  map->size = size;
  map->map_type = SL_MMAP_SHM;
  map->gbm_map_data = nullptr;
  map->gbmbo = nullptr;
  map->num_planes = num_planes;
  map->bpp = bpp;
  map->offset[0] = offset0;
  map->stride[0] = stride0;
  map->offset[1] = offset1;
  map->stride[1] = stride1;
  map->y_ss[0] = y_ss0;
  map->y_ss[1] = y_ss1;
  map->begin_write = nullptr;
  map->end_write = nullptr;
  map->buffer_resource = nullptr;
  map->addr =
      mmap(nullptr, size + offset0, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  errno_assert(map->addr != MAP_FAILED);

  return map;
}

struct sl_mmap* sl_drm_prime_mmap_create(gbm_device* device,
                                         int fd,
                                         size_t bpp,
                                         size_t num_planes,
                                         size_t stride,
                                         int32_t width,
                                         int32_t height,
                                         uint32_t drm_format) {
  TRACE_EVENT("drm", "sl_drm_mmap_create");
  struct sl_mmap* map = new sl_mmap();
  assert(num_planes == 1);

  map->refcount = 1;
  map->fd = fd;
  map->num_planes = num_planes;
  map->bpp = bpp;
  map->offset[0] = 0;
  map->stride[0] = stride;
  map->offset[1] = 0;
  map->stride[1] = 0;
  map->y_ss[0] = 1;
  map->y_ss[1] = 1;
  map->begin_write = nullptr;
  map->end_write = nullptr;
  map->buffer_resource = nullptr;
  map->map_type = SL_MMAP_DRM_PRIME;
  map->gbm_map_data = nullptr;
  map->addr = nullptr;
  map->gbmbo = nullptr;

  // Prefill in the gbm_import_data structure
  map->gbm_import_data.fd = fd;
  map->gbm_import_data.width = width;
  map->gbm_import_data.height = height;
  map->gbm_import_data.stride = stride;
  map->gbm_import_data.format = drm_format;

  // Save the device as we will need it later
  map->gbm_device_object = device;
  return map;
}

struct sl_mmap* sl_mmap_ref(struct sl_mmap* map) {
  TRACE_EVENT("shm", "sl_mmap_ref");
  map->refcount++;
  return map;
}

bool sl_mmap_begin_access(struct sl_mmap* map) {
  uint32_t ret_stride;

  // This function is to be used on the DRM PRIME mmap path.
  // It is used to ensure we can actually access the resource
  // when its contents are needed.
  // If we have any other mmap type, we should simply return true
  // under the assumption that they do not need to perform this extra
  // check
  if (map->map_type != SL_MMAP_DRM_PRIME)
    return true;

  // Attempt to import (and map) the GBM BO
  // If we cannot do so, return false so the upper layers
  // can respond appropriately.
  map->gbmbo = gbm_bo_import(map->gbm_device_object, GBM_BO_IMPORT_FD,
                             reinterpret_cast<void*>(&map->gbm_import_data),
                             GBM_BO_USE_LINEAR);
  if (!map->gbmbo) {
    return false;
  }

  map->gbm_map_data = nullptr;
  map->addr = gbm_bo_map(map->gbmbo, 0, 0, map->gbm_import_data.width,
                         map->gbm_import_data.height, GBM_BO_TRANSFER_READ,
                         &ret_stride, &map->gbm_map_data);
  if (!map->addr) {
    gbm_bo_destroy(map->gbmbo);
    return false;
  }

  map->stride[0] = ret_stride;
  map->size = ret_stride * map->gbm_import_data.height;

  return true;
}

void sl_mmap_end_access(struct sl_mmap* map) {
  if (map->map_type != SL_MMAP_DRM_PRIME)
    return;

  if (map->addr && map->gbm_map_data) {
    gbm_bo_unmap(map->gbmbo, map->gbm_map_data);
    map->addr = nullptr;
    map->gbm_map_data = nullptr;
  }

  if (map->gbmbo) {
    gbm_bo_destroy(map->gbmbo);
    map->gbmbo = nullptr;
  }
}

void sl_mmap_unref(struct sl_mmap* map) {
  TRACE_EVENT("shm", "sl_mmap_unref");

  map->refcount--;
  assert(map->refcount >= 0);

  if (map->refcount == 0) {
    switch (map->map_type) {
      case SL_MMAP_SHM:
        munmap(map->addr, map->size + map->offset[0]);
        if (map->fd != -1)
          close(map->fd);
        delete map;
        break;

      case SL_MMAP_DRM_PRIME:
        // Invoke end_access just in case
        sl_mmap_end_access(map);
        if (map->fd != -1)
          close(map->fd);
        delete map;
        break;

      default:
        break;
    }
  }
}
