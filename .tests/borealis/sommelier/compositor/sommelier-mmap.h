// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_MMAP_H_
#define VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_MMAP_H_

#include <gbm.h>
#include <sys/types.h>

typedef void (*sl_begin_end_access_func_t)(int fd, struct sl_context* ctx);

enum slMmapType {
  SL_MMAP_NONE,      // None
  SL_MMAP_SHM,       // SHM mmap type
  SL_MMAP_DRM_PRIME  // DRM PRIME mmap type
};

struct sl_mmap {
  int refcount;
  int fd;
  void* addr;
  struct gbm_bo* gbmbo;
  void* gbm_map_data;
  gbm_device* gbm_device_object;
  size_t size;
  size_t bpp;
  size_t num_planes;
  size_t offset[2];
  size_t stride[2];
  size_t y_ss[2];
  sl_begin_end_access_func_t begin_write;
  sl_begin_end_access_func_t end_write;
  slMmapType map_type;
  struct gbm_import_fd_data gbm_import_data;
  struct wl_resource* buffer_resource;
};

struct sl_mmap* sl_drm_prime_mmap_create(gbm_device* device,
                                         int fd,
                                         size_t bpp,
                                         size_t num_planes,
                                         size_t stride,
                                         int32_t width,
                                         int32_t height,
                                         uint32_t drm_format);

struct sl_mmap* sl_mmap_create(int fd,
                               size_t size,
                               size_t bpp,
                               size_t num_planes,
                               size_t offset0,
                               size_t stride0,
                               size_t offset1,
                               size_t stride1,
                               size_t y_ss0,
                               size_t y_ss1);

bool sl_mmap_begin_access(struct sl_mmap* map);
void sl_mmap_end_access(struct sl_mmap* map);

struct sl_mmap* sl_mmap_ref(struct sl_mmap* map);
void sl_mmap_unref(struct sl_mmap* map);

#endif  // VM_TOOLS_SOMMELIER_COMPOSITOR_SOMMELIER_MMAP_H_
