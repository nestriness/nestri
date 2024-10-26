// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../sommelier.h"          // NOLINT(build/include_directory)
#include "../sommelier-tracing.h"  // NOLINT(build/include_directory)
#include "../sommelier-util.h"     // NOLINT(build/include_directory)
#include "sommelier-formats.h"     // NOLINT(build/include_directory)

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-client.h>

#include "drm-server-protocol.h"  // NOLINT(build/include_directory)
#include "linux-dmabuf-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)

struct sl_host_shm_pool {
  struct sl_shm* shm;
  struct wl_resource* resource;
  struct wl_shm_pool* proxy;
  int fd;
};

struct sl_host_shm {
  struct sl_shm* shm;
  struct wl_resource* resource;
  struct wl_shm* shm_proxy;
  struct zwp_linux_dmabuf_v1* linux_dmabuf_proxy;
};

static void sl_host_shm_pool_create_host_buffer(struct wl_client* client,
                                                struct wl_resource* resource,
                                                uint32_t id,
                                                int32_t offset,
                                                int32_t width,
                                                int32_t height,
                                                int32_t stride,
                                                uint32_t format) {
  struct sl_host_shm_pool* host =
      static_cast<sl_host_shm_pool*>(wl_resource_get_user_data(resource));

  if (host->shm->ctx->channel == nullptr) {
    // Running in noop mode, without virtualization.
    assert(host->proxy);
    sl_create_host_buffer(host->shm->ctx, client, id,
                          wl_shm_pool_create_buffer(host->proxy, offset, width,
                                                    height, stride, format),
                          width, height, /*is_drm=*/true);
    return;
  }

  struct sl_host_buffer* host_buffer = sl_create_host_buffer(
      host->shm->ctx, client, id, nullptr, width, height, /*is_drm=*/false);

  host_buffer->shm_format = format;
  host_buffer->shm_mmap = sl_mmap_create(
      host->fd, sl_shm_format_size(format, height, stride),
      sl_shm_format_bpp(format), sl_shm_format_num_planes(format), offset,
      stride, offset + sl_shm_format_plane_offset(format, 1, height, stride),
      stride, sl_shm_format_plane_y_subsampling(format, 0),
      sl_shm_format_plane_y_subsampling(format, 1));
  // In the case of mmaps created from the client buffer, we want to be able
  // to close the FD when the client releases the shm pool (i.e. when it's
  // done transferring) as opposed to when the pool is freed (i.e. when we're
  // done drawing).
  // We do this by removing the handle to the FD after it has been mmapped,
  // which prevents a double-close.
  host_buffer->shm_mmap->fd = -1;
  host_buffer->shm_mmap->buffer_resource = host_buffer->resource;
}

static void sl_host_shm_pool_destroy(struct wl_client* client,
                                     struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_host_shm_pool_resize(struct wl_client* client,
                                    struct wl_resource* resource,
                                    int32_t size) {
  struct sl_host_shm_pool* host =
      static_cast<sl_host_shm_pool*>(wl_resource_get_user_data(resource));

  if (host->proxy)
    wl_shm_pool_resize(host->proxy, size);
}

static const struct wl_shm_pool_interface sl_shm_pool_implementation = {
    sl_host_shm_pool_create_host_buffer, sl_host_shm_pool_destroy,
    sl_host_shm_pool_resize};

static void sl_destroy_host_shm_pool(struct wl_resource* resource) {
  struct sl_host_shm_pool* host =
      static_cast<sl_host_shm_pool*>(wl_resource_get_user_data(resource));

  if (host->fd >= 0)
    close(host->fd);
  if (host->proxy)
    wl_shm_pool_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_shm_create_host_pool(struct wl_client* client,
                                    struct wl_resource* resource,
                                    uint32_t id,
                                    int fd,
                                    int32_t size) {
  struct sl_host_shm* host =
      static_cast<sl_host_shm*>(wl_resource_get_user_data(resource));
  struct sl_host_shm_pool* host_shm_pool = new sl_host_shm_pool();

  host_shm_pool->shm = host->shm;
  host_shm_pool->fd = -1;
  host_shm_pool->proxy = nullptr;
  host_shm_pool->resource =
      wl_resource_create(client, &wl_shm_pool_interface, 1, id);
  wl_resource_set_implementation(host_shm_pool->resource,
                                 &sl_shm_pool_implementation, host_shm_pool,
                                 sl_destroy_host_shm_pool);

  if (host->shm->ctx->channel == nullptr) {
    // Running in noop mode, without virtualization.
    host_shm_pool->proxy = wl_shm_create_pool(host->shm_proxy, fd, size);
    wl_shm_pool_set_user_data(host_shm_pool->proxy, host_shm_pool);
    close(fd);
  } else {
    host_shm_pool->fd = fd;
  }
}

static const struct wl_shm_interface sl_shm_implementation = {
    sl_shm_create_host_pool};

static void sl_shm_format(void* data, struct wl_shm* shm, uint32_t format) {
  TRACE_EVENT("shm", "sl_shm_format");
  struct sl_host_shm* host =
      static_cast<sl_host_shm*>(wl_shm_get_user_data(shm));

  switch (format) {
    case WL_SHM_FORMAT_RGB565:
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_ABGR8888:
    case WL_SHM_FORMAT_XRGB8888:
    case WL_SHM_FORMAT_XBGR8888:
      wl_shm_send_format(host->resource, format);
      break;
    default:
      break;
  }
}

static const struct wl_shm_listener sl_shm_listener = {sl_shm_format};

static void sl_drm_format(void* data,
                          struct zwp_linux_dmabuf_v1* linux_dmabuf,
                          uint32_t format) {
  struct sl_host_shm* host = static_cast<sl_host_shm*>(
      zwp_linux_dmabuf_v1_get_user_data(linux_dmabuf));

  // Forward SHM versions of supported formats.
  switch (format) {
    case WL_DRM_FORMAT_NV12:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_NV12);
      break;
    case WL_DRM_FORMAT_RGB565:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_RGB565);
      break;
    case WL_DRM_FORMAT_ARGB8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_ARGB8888);
      break;
    case WL_DRM_FORMAT_ABGR8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_ABGR8888);
      break;
    case WL_DRM_FORMAT_XRGB8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_XRGB8888);
      break;
    case WL_DRM_FORMAT_XBGR8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_XBGR8888);
      break;
  }
}

static const struct zwp_linux_dmabuf_v1_listener sl_linux_dmabuf_listener = {
    sl_drm_format, /*modifier=*/DoNothing};

static void sl_destroy_host_shm(struct wl_resource* resource) {
  struct sl_host_shm* host =
      static_cast<sl_host_shm*>(wl_resource_get_user_data(resource));

  if (host->shm_proxy)
    wl_shm_destroy(host->shm_proxy);
  if (host->linux_dmabuf_proxy)
    zwp_linux_dmabuf_v1_destroy(host->linux_dmabuf_proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_bind_host_shm(struct wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_shm* host = new sl_host_shm();
  host->shm = ctx->shm;
  host->shm_proxy = nullptr;
  host->linux_dmabuf_proxy = nullptr;
  host->resource = wl_resource_create(client, &wl_shm_interface, 1, id);
  wl_resource_set_implementation(host->resource, &sl_shm_implementation, host,
                                 sl_destroy_host_shm);

  if (ctx->channel != nullptr && ctx->channel->supports_dmabuf()) {
    assert(ctx->linux_dmabuf);
    host->linux_dmabuf_proxy = static_cast<zwp_linux_dmabuf_v1*>(
        wl_registry_bind(wl_display_get_registry(ctx->display),
                         ctx->linux_dmabuf->id, &zwp_linux_dmabuf_v1_interface,
                         ZWP_LINUX_DMABUF_V1_FORMAT_SINCE_VERSION));
    zwp_linux_dmabuf_v1_add_listener(host->linux_dmabuf_proxy,
                                     &sl_linux_dmabuf_listener, host);
  } else {
    host->shm_proxy = static_cast<wl_shm*>(wl_registry_bind(
        wl_display_get_registry(ctx->display), ctx->shm->id, &wl_shm_interface,
        wl_resource_get_version(host->resource)));
    wl_shm_add_listener(host->shm_proxy, &sl_shm_listener, host);
  }
}

struct sl_global* sl_shm_global_create(struct sl_context* ctx) {
  return sl_global_create(ctx, &wl_shm_interface, 1, ctx, sl_bind_host_shm);
}
