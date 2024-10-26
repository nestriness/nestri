// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../sommelier.h"            // NOLINT(build/include_directory)
#include "../sommelier-tracing.h"    // NOLINT(build/include_directory)
#include "../sommelier-util.h"       // NOLINT(build/include_directory)
#include "sommelier-linux-dmabuf.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <gbm.h>
#include <libdrm/drm_fourcc.h>
#include <stdlib.h>
#include <unistd.h>
#include <xf86drm.h>

#include "drm-server-protocol.h"  // NOLINT(build/include_directory)
#include "linux-dmabuf-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)

#define SL_DRM_MAX_VERSION 2u

struct sl_host_drm {
  struct sl_context* ctx;
  uint32_t version;
  struct wl_resource* resource;
  struct zwp_linux_dmabuf_v1* linux_dmabuf_proxy;
  struct wl_callback* callback;
};

static void sl_drm_authenticate(struct wl_client* client,
                                struct wl_resource* resource,
                                uint32_t id) {
  TRACE_EVENT("drm", "sl_drm_authenticate");
  wl_drm_send_authenticated(resource);
}

static void sl_drm_create_buffer(struct wl_client* client,
                                 struct wl_resource* resource,
                                 uint32_t id,
                                 uint32_t name,
                                 int32_t width,
                                 int32_t height,
                                 uint32_t stride,
                                 uint32_t format) {
  assert(0);
}

static void sl_drm_create_planar_buffer(struct wl_client* client,
                                        struct wl_resource* resource,
                                        uint32_t id,
                                        uint32_t name,
                                        int32_t width,
                                        int32_t height,
                                        uint32_t format,
                                        int32_t offset0,
                                        int32_t stride0,
                                        int32_t offset1,
                                        int32_t stride1,
                                        int32_t offset2,
                                        int32_t stride2) {
  assert(0);
}

static void sl_drm_create_prime_buffer(struct wl_client* client,
                                       struct wl_resource* resource,
                                       uint32_t id,
                                       int32_t name,
                                       int32_t width,
                                       int32_t height,
                                       uint32_t format,
                                       int32_t offset0,
                                       int32_t stride0,
                                       int32_t offset1,
                                       int32_t stride1,
                                       int32_t offset2,
                                       int32_t stride2) {
  TRACE_EVENT("drm", "sl_drm_create_prime_buffer", "id", id);
  struct sl_host_drm* host =
      static_cast<sl_host_drm*>(wl_resource_get_user_data(resource));
  struct sl_context* ctx = host->ctx;
  struct zwp_linux_buffer_params_v1* buffer_params;

  assert(name >= 0);
  assert(stride0 >= 0);

  /* Sommelier's wl_drm has never supported multiplanar buffers and wl_drm is
   * nearing deprecation in favor of zwp_linux_dmabuf_v1. */
  if (offset1 || stride1 || offset2 || stride2) {
    wl_resource_post_error(host->resource, WL_DRM_ERROR_INVALID_NAME,
                           "multiplanar buffers not supported");
    return;
  }

  buffer_params = zwp_linux_dmabuf_v1_create_params(host->linux_dmabuf_proxy);

  uint32_t host_stride0 = stride0;
  uint32_t host_modifier_hi = DRM_FORMAT_MOD_INVALID >> 32;
  uint32_t host_modifier_lo = DRM_FORMAT_MOD_INVALID & 0xFFFFFFFF;
  bool is_virtgpu_buffer = false;
  if (ctx->gbm) {
    is_virtgpu_buffer = sl_linux_dmabuf_fixup_plane0_params(
        ctx->gbm, name, &host_stride0, &host_modifier_hi, &host_modifier_lo);
  }

  zwp_linux_buffer_params_v1_add(buffer_params, name, 0, offset0, host_stride0,
                                 host_modifier_hi, host_modifier_lo);

  struct wl_buffer* buffer_proxy = zwp_linux_buffer_params_v1_create_immed(
      buffer_params, width, height, format, 0);

  const struct sl_linux_dmabuf_host_buffer_create_info create_info = {
      .width = (uint32_t)width,
      .height = (uint32_t)height,
      .stride = host_stride0,
      .format = format,
      .dmabuf_fd = name,
      .is_virtgpu_buffer = is_virtgpu_buffer,
  };
  sl_linux_dmabuf_create_host_buffer(ctx, client, buffer_proxy, id,
                                     &create_info);

  zwp_linux_buffer_params_v1_destroy(buffer_params);
}

static const struct wl_drm_interface sl_drm_implementation = {
    sl_drm_authenticate, sl_drm_create_buffer, sl_drm_create_planar_buffer,
    sl_drm_create_prime_buffer};

static void sl_destroy_host_drm(struct wl_resource* resource) {
  struct sl_host_drm* host =
      static_cast<sl_host_drm*>(wl_resource_get_user_data(resource));

  zwp_linux_dmabuf_v1_destroy(host->linux_dmabuf_proxy);
  wl_callback_destroy(host->callback);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_drm_format(void* data,
                          struct zwp_linux_dmabuf_v1* linux_dmabuf,
                          uint32_t format) {
  struct sl_host_drm* host = static_cast<sl_host_drm*>(
      zwp_linux_dmabuf_v1_get_user_data(linux_dmabuf));

  switch (format) {
    case WL_DRM_FORMAT_RGB565:
    case WL_DRM_FORMAT_ARGB8888:
    case WL_DRM_FORMAT_ABGR8888:
    case WL_DRM_FORMAT_XRGB8888:
    case WL_DRM_FORMAT_XBGR8888:
      wl_drm_send_format(host->resource, format);
      break;
    default:
      break;
  }
}

static const struct zwp_linux_dmabuf_v1_listener sl_linux_dmabuf_listener = {
    sl_drm_format, /*modifier=*/DoNothing};

static void sl_drm_callback_done(void* data,
                                 struct wl_callback* callback,
                                 uint32_t serial) {
  struct sl_host_drm* host =
      static_cast<sl_host_drm*>(wl_callback_get_user_data(callback));

  if (host->ctx->drm_device)
    wl_drm_send_device(host->resource, host->ctx->drm_device);
  if (host->version >= WL_DRM_CREATE_PRIME_BUFFER_SINCE_VERSION)
    wl_drm_send_capabilities(host->resource, WL_DRM_CAPABILITY_PRIME);
}

static const struct wl_callback_listener sl_drm_callback_listener = {
    sl_drm_callback_done};

static void sl_bind_host_drm(struct wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  struct sl_linux_dmabuf* linux_dmabuf =
      static_cast<struct sl_linux_dmabuf*>(data);
  struct sl_context* ctx = linux_dmabuf->ctx;

  struct sl_host_drm* host = new sl_host_drm();
  host->ctx = ctx;
  host->version = MIN(version, SL_DRM_MAX_VERSION);
  host->resource =
      wl_resource_create(client, &wl_drm_interface, host->version, id);
  wl_resource_set_implementation(host->resource, &sl_drm_implementation, host,
                                 sl_destroy_host_drm);

  assert(linux_dmabuf->version >=
         ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED_SINCE_VERSION);
  host->linux_dmabuf_proxy = static_cast<zwp_linux_dmabuf_v1*>(
      wl_registry_bind(wl_display_get_registry(ctx->display), linux_dmabuf->id,
                       &zwp_linux_dmabuf_v1_interface,
                       ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED_SINCE_VERSION));
  zwp_linux_dmabuf_v1_add_listener(host->linux_dmabuf_proxy,
                                   &sl_linux_dmabuf_listener, host);

  host->callback = wl_display_sync(ctx->display);
  wl_callback_add_listener(host->callback, &sl_drm_callback_listener, host);
}

struct sl_global* sl_drm_global_create(struct sl_context* ctx,
                                       sl_linux_dmabuf* linux_dmabuf) {
  // Early out if DMABuf protocol version is not sufficient.
  if (linux_dmabuf->version <
      ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED_SINCE_VERSION) {
    return nullptr;
  }

  return sl_global_create(ctx, &wl_drm_interface, SL_DRM_MAX_VERSION,
                          reinterpret_cast<void*>(linux_dmabuf),
                          sl_bind_host_drm);
}
