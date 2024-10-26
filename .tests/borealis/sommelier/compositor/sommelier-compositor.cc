// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../sommelier.h"            // NOLINT(build/include_directory)
#include "../sommelier-logging.h"    // NOLINT(build/include_directory)
#include "../sommelier-timing.h"     // NOLINT(build/include_directory)
#include "../sommelier-tracing.h"    // NOLINT(build/include_directory)
#include "../sommelier-transform.h"  // NOLINT(build/include_directory)
#include "../sommelier-window.h"     // NOLINT(build/include_directory)
#include "../sommelier-xshape.h"     // NOLINT(build/include_directory)
#include "sommelier-dmabuf-sync.h"   // NOLINT(build/include_directory)
#include "sommelier-formats.h"       // NOLINT(build/include_directory)
#include "viewporter-shim.h"         // NOLINT(build/include_directory)
#include <assert.h>
#include <errno.h>
#include <libdrm/drm_fourcc.h>
#include <limits.h>
#include <pixman.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "drm-server-protocol.h"  // NOLINT(build/include_directory)
#include "linux-dmabuf-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "linux-explicit-synchronization-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "viewporter-client-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-shell-client-protocol.h"   // NOLINT(build/include_directory)

struct sl_host_compositor {
  struct sl_compositor* compositor;
  struct wl_resource* resource;
  struct wl_compositor* proxy;
};

struct sl_output_buffer {
  struct wl_list link;
  uint32_t width;
  uint32_t height;
  uint32_t format;
  struct wl_buffer* internal;
  struct sl_mmap* mmap;
  struct pixman_region32 surface_damage;
  struct pixman_region32 buffer_damage;
  pixman_image_t* shape_image;
  struct sl_host_surface* surface;
};

static void sl_virtwl_dmabuf_sync(int fd, __u32 flags, struct sl_context* ctx) {
  int rv;
  rv = ctx->channel->sync(fd, flags);
  assert(!rv);
  UNUSED(rv);
}

static void sl_virtwl_dmabuf_begin_write(int fd, struct sl_context* ctx) {
  sl_virtwl_dmabuf_sync(fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE, ctx);
}

static void sl_virtwl_dmabuf_end_write(int fd, struct sl_context* ctx) {
  sl_virtwl_dmabuf_sync(fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE, ctx);
}

static void sl_output_buffer_destroy(struct sl_output_buffer* buffer) {
  wl_buffer_destroy(buffer->internal);
  sl_mmap_unref(buffer->mmap);
  pixman_region32_fini(&buffer->surface_damage);
  pixman_region32_fini(&buffer->buffer_damage);
  wl_list_remove(&buffer->link);

  if (buffer->shape_image)
    pixman_image_unref(buffer->shape_image);

  delete buffer;
}

static uint32_t try_wl_resource_get_id(wl_resource* resource) {
  return resource ? wl_resource_get_id(resource) : -1;
}

static void sl_output_buffer_release(void* data, struct wl_buffer* buffer) {
  struct sl_output_buffer* output_buffer =
      static_cast<sl_output_buffer*>(wl_buffer_get_user_data(buffer));
  TRACE_EVENT("surface", "sl_output_buffer_release", "resource_id",
              try_wl_resource_get_id(output_buffer->surface->resource));
  struct sl_host_surface* host_surface = output_buffer->surface;

  wl_list_remove(&output_buffer->link);
  wl_list_insert(&host_surface->released_buffers, &output_buffer->link);
}

static const struct wl_buffer_listener sl_output_buffer_listener = {
    sl_output_buffer_release};

static void sl_host_surface_destroy(struct wl_client* client,
                                    struct wl_resource* resource) {
  TRACE_EVENT("surface", "sl_host_surface_destroy", "resource_id",
              try_wl_resource_get_id(resource));
  wl_resource_destroy(resource);
}

static void sl_host_surface_attach(struct wl_client* client,
                                   struct wl_resource* resource,
                                   struct wl_resource* buffer_resource,
                                   int32_t x,
                                   int32_t y) {
  auto resource_id = wl_resource_get_id(resource);
  auto buffer_id =
      buffer_resource ? wl_resource_get_id(buffer_resource) : kUnknownBufferId;
  TRACE_EVENT("surface", "sl_host_surface_attach", "resource_id", resource_id,
              "buffer_id", buffer_id);
  struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(resource));
  if (host->ctx->timing != nullptr) {
    host->ctx->timing->UpdateLastAttach(resource_id, buffer_id);
  }
  struct sl_host_buffer* host_buffer =
      buffer_resource ? static_cast<sl_host_buffer*>(
                            wl_resource_get_user_data(buffer_resource))
                      : nullptr;
  struct wl_buffer* buffer_proxy = nullptr;
  struct sl_window* window = nullptr;
  bool window_shaped = false;

  // The window_shaped flag should only be set if the xshape functionality
  // has been explicitly enabled
  window = sl_context_lookup_window_for_surface(host->ctx, resource);
  if (window && host->ctx->enable_xshape)
    window_shaped = window->shaped;

  host->current_buffer = nullptr;
  if (host->contents_shm_mmap) {
    sl_mmap_unref(host->contents_shm_mmap);
    host->contents_shm_mmap = nullptr;
  }

  if (host_buffer) {
    host->contents_width = host_buffer->width;
    host->contents_height = host_buffer->height;
    host->contents_shm_format = host_buffer->shm_format;
    host->proxy_buffer = host_buffer->proxy;
    buffer_proxy = host_buffer->proxy;

    if (!host_buffer->is_drm) {
      if (host_buffer->shm_mmap)
        host->contents_shm_mmap = sl_mmap_ref(host_buffer->shm_mmap);
    } else {
      if (window_shaped && host_buffer->shm_mmap) {
        host->contents_shm_mmap = sl_mmap_ref(host_buffer->shm_mmap);
      } else {
        // A fallback if for some reason the DRM PRIME mmap container was
        // not created (or if this is not a shaped window)
        host->contents_shm_mmap = nullptr;
        window_shaped = false;
      }
    }
  }

  // Transfer the flag and shape data over to the surface
  // if we are working on a shaped window
  if (window_shaped) {
    assert(sl_shm_format_is_supported(host_buffer->shm_format));
    host->contents_shaped = true;
    pixman_region32_copy(&host->contents_shape, &window->shape_rectangles);
  } else {
    host->contents_shaped = false;
    pixman_region32_clear(&host->contents_shape);
  }

  // This code will also check if we need to reallocate
  // a output_surface based on the status of the shaped flag
  //
  // An output_surface that is shaped will have its format
  // forced to ARGB8888 (hence the changes below)
  if (host->contents_shm_mmap) {
    while (!wl_list_empty(&host->released_buffers)) {
      host->current_buffer = wl_container_of(host->released_buffers.next,
                                             host->current_buffer, link);

      if (host->current_buffer->width == host_buffer->width &&
          host->current_buffer->height == host_buffer->height &&
          ((window_shaped && host->current_buffer->shape_image &&
            host->current_buffer->format == WL_SHM_FORMAT_ARGB8888) ||
           (!window_shaped &&
            host->current_buffer->format == host_buffer->shm_format))) {
        break;
      }

      sl_output_buffer_destroy(host->current_buffer);
      host->current_buffer = nullptr;
    }

    // Allocate new output buffer.
    if (!host->current_buffer) {
      TRACE_EVENT("surface", "sl_host_surface_attach: allocate_buffer",
                  "dmabuf_enabled", host->ctx->channel->supports_dmabuf());
      size_t width = host_buffer->width;
      size_t height = host_buffer->height;
      uint32_t shm_format =
          window_shaped ? WL_SHM_FORMAT_ARGB8888 : host_buffer->shm_format;
      size_t bpp = sl_shm_format_bpp(shm_format);
      size_t num_planes = sl_shm_format_num_planes(shm_format);

      host->current_buffer = new sl_output_buffer();
      wl_list_insert(&host->released_buffers, &host->current_buffer->link);
      host->current_buffer->width = width;
      host->current_buffer->height = height;
      host->current_buffer->format = shm_format;
      host->current_buffer->surface = host;
      pixman_region32_init_rect(&host->current_buffer->surface_damage, 0, 0,
                                MAX_SIZE, MAX_SIZE);
      pixman_region32_init_rect(&host->current_buffer->buffer_damage, 0, 0,
                                MAX_SIZE, MAX_SIZE);

      if (window_shaped) {
        host->current_buffer->shape_image = pixman_image_create_bits_no_clear(
            PIXMAN_a8r8g8b8, width, height, nullptr, 0);

        assert(host->current_buffer->shape_image);
      } else {
        host->current_buffer->shape_image = nullptr;
      }

      if (host->ctx->channel->supports_dmabuf() &&
          host->ctx->linux_dmabuf->proxy_v2) {
        int rv;
        size_t size;
        struct zwp_linux_buffer_params_v1* buffer_params;
        struct WaylandBufferCreateInfo create_info = {};
        struct WaylandBufferCreateOutput create_output = {};
        create_info.dmabuf = true;

        create_info.width = static_cast<__u32>(width);
        create_info.height = static_cast<__u32>(height);
        create_info.drm_format = sl_shm_format_to_drm_format(shm_format);

        rv = host->ctx->channel->allocate(create_info, create_output);
        if (rv) {
          LOG(FATAL) << "virtwl dmabuf allocation failed: " << strerror(-rv);
          _exit(EXIT_FAILURE);
        }

        size = create_output.host_size;
        buffer_params = zwp_linux_dmabuf_v1_create_params(
            host->ctx->linux_dmabuf->proxy_v2);
        zwp_linux_buffer_params_v1_add(buffer_params, create_output.fd, 0,
                                       create_output.offsets[0],
                                       create_output.strides[0], 0, 0);
        if (num_planes > 1) {
          zwp_linux_buffer_params_v1_add(buffer_params, create_output.fd, 1,
                                         create_output.offsets[1],
                                         create_output.strides[1], 0, 0);
          size = MAX(size, create_output.offsets[1] +
                               create_output.offsets[1] * height /
                                   host->contents_shm_mmap->y_ss[1]);
        }
        host->current_buffer->internal =
            zwp_linux_buffer_params_v1_create_immed(
                buffer_params, width, height, create_info.drm_format, 0);
        zwp_linux_buffer_params_v1_destroy(buffer_params);

        host->current_buffer->mmap = sl_mmap_create(
            create_output.fd, size, bpp, num_planes, create_output.offsets[0],
            create_output.strides[0], create_output.offsets[1],
            create_output.strides[1], host->contents_shm_mmap->y_ss[0],
            host->contents_shm_mmap->y_ss[1]);
        host->current_buffer->mmap->begin_write = sl_virtwl_dmabuf_begin_write;
        host->current_buffer->mmap->end_write = sl_virtwl_dmabuf_end_write;
      } else {
        size_t size = host->contents_shm_mmap->size;
        struct WaylandBufferCreateInfo create_info = {};
        struct WaylandBufferCreateOutput create_output = {};
        struct wl_shm_pool* pool;
        int rv;

        create_info.drm_format = DRM_FORMAT_R8;
        create_info.height = 1;
        create_info.width = size;
        create_info.size = static_cast<__u32>(size);

        rv = host->ctx->channel->allocate(create_info, create_output);
        UNUSED(rv);

        pool = wl_shm_create_pool(host->ctx->shm->internal, create_output.fd,
                                  create_output.host_size);

        host->current_buffer->internal = wl_shm_pool_create_buffer(
            pool, 0, width, height, host->contents_shm_mmap->stride[0],
            shm_format);
        wl_shm_pool_destroy(pool);

        host->current_buffer->mmap = sl_mmap_create(
            create_output.fd, create_output.host_size, bpp, num_planes, 0,
            host->contents_shm_mmap->stride[0],
            host->contents_shm_mmap->offset[1] -
                host->contents_shm_mmap->offset[0],
            host->contents_shm_mmap->stride[1],
            host->contents_shm_mmap->y_ss[0], host->contents_shm_mmap->y_ss[1]);
      }

      assert(host->current_buffer->internal);
      assert(host->current_buffer->mmap);

      wl_buffer_add_listener(host->current_buffer->internal,
                             &sl_output_buffer_listener, host->current_buffer);
    }
  }

  sl_transform_guest_to_host(host->ctx, host, &x, &y);

  // Save these values in case we need to roll back the decisions
  // made here in the commit phase of the attach-commit cycle
  host->contents_x_offset = x;
  host->contents_y_offset = y;

  if (host_buffer && host_buffer->sync_point) {
    TRACE_EVENT("surface", "sl_host_surface_attach: sync_point");

    bool needs_sync = true;
    if (host->surface_sync) {
      int ret;
      int sync_file_fd;

      ret = sl_dmabuf_get_read_sync_file(host_buffer->sync_point->fd,
                                         sync_file_fd);
      if (ret) {
        if (ret == ENOTTY) {
          // export sync file ioctl not implemented. Revert to previous method
          // of guest side sync going forward.
          zwp_linux_surface_synchronization_v1_destroy(host->surface_sync);
          host->surface_sync = nullptr;
          LOG(INFO) << "DMA_BUF_IOCTL_EXPORT_SYNC_FILE not implemented, "
                       "defaulting to implicit fence for synchronization";
        } else {
          LOG(WARNING) << "Explicit synchronization failed with reason: "
                       << strerror(ret) << ", will retry on next attach";
        }
      } else {
        if (sl_dmabuf_sync_is_virtgpu(sync_file_fd)) {
          // virtio_gpu fences can be translated and forwarded to host.
          zwp_linux_surface_synchronization_v1_set_acquire_fence(
              host->surface_sync, sync_file_fd);
        } else {
          // TODO(dbehr) b/237014660. Create a host proxy fence for this fence
          // instead of waiting for rendering to finish here.
          TRACE_EVENT("surface", "sl_host_surface_attach: sync_wait",
                      "prime_fd", host_buffer->sync_point->fd);
          sl_dmabuf_sync_wait(sync_file_fd);
        }
        needs_sync = false;
        close(sync_file_fd);
      }
    }

    if (needs_sync)
      host_buffer->sync_point->sync(host->ctx, host_buffer->sync_point);
  }

  if (host->current_buffer) {
    assert(host->current_buffer->internal);
    wl_surface_attach(host->proxy, host->current_buffer->internal, x, y);
  } else {
    wl_surface_attach(host->proxy, buffer_proxy, x, y);
  }

  wl_list_for_each(window, &host->ctx->windows, link) {
    if (window->host_surface_id == try_wl_resource_get_id(resource)) {
      while (sl_process_pending_configure_acks(window, host))
        continue;

      break;
    }
  }
}

// Return the scale and offset from surface coordinates to buffer pixel
// coordinates, taking the viewport into account (if any).
void compute_buffer_scale_and_offset(const sl_host_surface* host,
                                     const sl_viewport* viewport,
                                     double* out_scale_x,
                                     double* out_scale_y,
                                     wl_fixed_t* out_offset_x,
                                     wl_fixed_t* out_offset_y) {
  double scale_x = host->contents_scale;
  double scale_y = host->contents_scale;
  wl_fixed_t offset_x = 0;
  wl_fixed_t offset_y = 0;
  if (viewport) {
    double contents_width = host->contents_width;
    double contents_height = host->contents_height;

    if (viewport->src_x >= 0 && viewport->src_y >= 0) {
      offset_x = viewport->src_x;
      offset_y = viewport->src_y;
    }

    if (viewport->dst_width > 0 && viewport->dst_height > 0) {
      scale_x *= contents_width / viewport->dst_width;
      scale_y *= contents_height / viewport->dst_height;

      // Take source rectangle into account when both destination size and
      // source rectangle are set. If only source rectangle is set, then
      // it determines the surface size so it can be ignored.
      if (viewport->src_width >= 0 && viewport->src_height >= 0) {
        scale_x *= wl_fixed_to_double(viewport->src_width) / contents_width;
        scale_y *= wl_fixed_to_double(viewport->src_height) / contents_height;
      }
    }
  }
  *out_scale_x = scale_x;
  *out_scale_y = scale_y;
  *out_offset_x = offset_x;
  *out_offset_y = offset_y;
}

static void sl_host_surface_damage(struct wl_client* client,
                                   struct wl_resource* resource,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  TRACE_EVENT("surface", "sl_host_surface_damage", "resource_id",
              try_wl_resource_get_id(resource));
  const struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(resource));

  struct sl_output_buffer* buffer;
  int64_t x1, y1, x2, y2;

  wl_list_for_each(buffer, &host->busy_buffers, link) {
    pixman_region32_union_rect(&buffer->surface_damage, &buffer->surface_damage,
                               x, y, width, height);
  }
  wl_list_for_each(buffer, &host->released_buffers, link) {
    pixman_region32_union_rect(&buffer->surface_damage, &buffer->surface_damage,
                               x, y, width, height);
  }

  x1 = x;
  y1 = y;
  x2 = x1 + width;
  y2 = y1 + height;

  sl_transform_damage_coord(host->ctx, host, 1.0, 1.0, &x1, &y1, &x2, &y2);
  wl_surface_damage(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void sl_host_surface_damage_buffer(struct wl_client* client,
                                          struct wl_resource* resource,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height) {
  TRACE_EVENT("surface", "sl_host_surface_damage_buffer", "resource_id",
              try_wl_resource_get_id(resource));
  const struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(resource));
  struct sl_output_buffer* buffer;

  wl_list_for_each(buffer, &host->busy_buffers, link) {
    pixman_region32_union_rect(&buffer->buffer_damage, &buffer->buffer_damage,
                               x, y, width, height);
  }
  wl_list_for_each(buffer, &host->released_buffers, link) {
    pixman_region32_union_rect(&buffer->buffer_damage, &buffer->buffer_damage,
                               x, y, width, height);
  }

  // Forward wl_surface_damage() call to the host. Since the damage region is
  // given in buffer pixel coordinates, convert to surface coordinates first.
  // If the host supports wl_surface_damage_buffer one day, we can avoid this
  // conversion.
  double scale_x, scale_y;
  wl_fixed_t offset_x, offset_y;
  struct sl_viewport* viewport = nullptr;
  if (!wl_list_empty(&host->contents_viewport))
    viewport = wl_container_of(host->contents_viewport.next, viewport, link);

  compute_buffer_scale_and_offset(host, viewport, &scale_x, &scale_y, &offset_x,
                                  &offset_y);

  int64_t x1 = x - wl_fixed_to_int(offset_x);
  int64_t y1 = y - wl_fixed_to_int(offset_y);
  int64_t x2 = x1 + width;
  int64_t y2 = y1 + height;

  sl_transform_damage_coord(host->ctx, host, scale_x, scale_y, &x1, &y1, &x2,
                            &y2);
  wl_surface_damage(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void sl_frame_callback_done(void* data,
                                   struct wl_callback* callback,
                                   uint32_t time) {
  TRACE_EVENT("surface", "sl_frame_callback_done");
  struct sl_host_callback* host =
      static_cast<sl_host_callback*>(wl_callback_get_user_data(callback));

  wl_callback_send_done(host->resource, time);
  wl_resource_destroy(host->resource);
}

static const struct wl_callback_listener sl_frame_callback_listener = {
    sl_frame_callback_done};

static void sl_host_callback_destroy(struct wl_resource* resource) {
  TRACE_EVENT("surface", "sl_host_callback_destroy");
  struct sl_host_callback* host =
      static_cast<sl_host_callback*>(wl_resource_get_user_data(resource));

  wl_callback_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_host_surface_frame(struct wl_client* client,
                                  struct wl_resource* resource,
                                  uint32_t callback) {
  TRACE_EVENT("surface", "sl_host_surface_frame", "resource_id",
              try_wl_resource_get_id(resource));
  struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(resource));
  struct sl_host_callback* host_callback = new sl_host_callback();

  host_callback->resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);
  wl_resource_set_implementation(host_callback->resource, nullptr,
                                 host_callback, sl_host_callback_destroy);
  host_callback->proxy = wl_surface_frame(host->proxy);
  wl_callback_add_listener(host_callback->proxy, &sl_frame_callback_listener,
                           host_callback);
}

static void copy_damaged_rect(sl_host_surface* host,
                              pixman_box32_t* rect,
                              bool shaped,
                              double scale_x,
                              double scale_y,
                              double offset_x,
                              double offset_y) {
  uint8_t* src_addr = static_cast<uint8_t*>(host->contents_shm_mmap->addr);
  uint8_t* dst_addr = static_cast<uint8_t*>(host->current_buffer->mmap->addr);
  size_t* src_offset = host->contents_shm_mmap->offset;
  size_t* dst_offset = host->current_buffer->mmap->offset;
  size_t* src_stride = host->contents_shm_mmap->stride;
  size_t* dst_stride = host->current_buffer->mmap->stride;
  size_t* y_ss = host->contents_shm_mmap->y_ss;
  size_t bpp = host->contents_shm_mmap->bpp;
  size_t num_planes = host->contents_shm_mmap->num_planes;
  int32_t x1, y1, x2, y2;
  size_t null_set[3] = {0, 0, 0};
  size_t shape_stride[3] = {0, 0, 0};

  if (shaped) {
    // If we are copying from a shaped window, the actual image data
    // we want comes from the shape_image. We are making the modifications
    // here so the source data comes from this buffer.
    src_addr = reinterpret_cast<uint8_t*>(
        pixman_image_get_data(host->current_buffer->shape_image));
    src_offset = null_set;
    src_stride = shape_stride;

    shape_stride[0] =
        pixman_image_get_stride(host->current_buffer->shape_image);
  }

  // Enclosing rect after applying scale and offset.
  x1 = rect->x1 * scale_x + offset_x;
  y1 = rect->y1 * scale_y + offset_y;
  x2 = rect->x2 * scale_x + offset_x + 0.5;
  y2 = rect->y2 * scale_y + offset_y + 0.5;

  x1 = MAX(0, x1);
  y1 = MAX(0, y1);
  x2 = MIN(static_cast<int32_t>(host->contents_width), x2);
  y2 = MIN(static_cast<int32_t>(host->contents_height), y2);

  if (x1 < x2 && y1 < y2) {
    size_t i;

    for (i = 0; i < num_planes; ++i) {
      uint8_t* src_base = src_addr + src_offset[i];
      uint8_t* dst_base = dst_addr + dst_offset[i];
      uint8_t* src = src_base + y1 * src_stride[i] + x1 * bpp;
      uint8_t* dst = dst_base + y1 * dst_stride[i] + x1 * bpp;
      int32_t width = x2 - x1;
      int32_t height = (y2 - y1) / y_ss[i];
      size_t bytes = width * bpp;

      while (height--) {
        memcpy(dst, src, bytes);
        dst += dst_stride[i];
        src += src_stride[i];
      }
    }
  }
}

void sl_host_surface_commit(struct wl_client* client,
                            struct wl_resource* resource) {
  auto resource_id = try_wl_resource_get_id(resource);
  TRACE_EVENT(
      "surface", "sl_host_surface_commit", "resource_id", resource_id,
      [&](perfetto::EventContext p) { perfetto_annotate_time_sync(p); });
  struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(resource));
  if (host->ctx->timing != nullptr) {
    host->ctx->timing->UpdateLastCommit(resource_id);
  }
  struct sl_window* window = host->window;
  struct sl_viewport* viewport = nullptr;

  if (!wl_list_empty(&host->contents_viewport))
    viewport = wl_container_of(host->contents_viewport.next, viewport, link);

  // We are doing this check outside the contents_shm_mmap check below so
  // we can bail out on the shaped processing of a DRM buffer in case mapping
  // it fails.
  if (host->contents_shm_mmap) {
    bool mmap_ensured = sl_mmap_begin_access(host->contents_shm_mmap);

    if (!mmap_ensured) {
      // Clean up anything left from begin_access
      sl_mmap_end_access(host->contents_shm_mmap);

      if (host->contents_shaped) {
        // Shaped contents, with a source buffer.
        // We need to fallback and disable shape processing
        // This might also be a good event to log somewhere
        sl_mmap_unref(host->contents_shm_mmap);

        // Release the allocated output buffer back to the queue
        host->contents_shm_mmap = nullptr;
        host->contents_shaped = false;
        pixman_region32_clear(&host->contents_shape);
        sl_output_buffer_release(nullptr, host->current_buffer->internal);

        // Attach the original buffer back, ensure proxy_buffer is not nullptr
        assert(host->proxy_buffer);
        wl_surface_attach(host->proxy, host->proxy_buffer,
                          host->contents_x_offset, host->contents_y_offset);
      } else {
        // If we are not shaped, we will still need access to the buffer in this
        // case. We shouldn't get here. We are using this assert to provide some
        // clues within error logs that may result from hitting this point.
        assert(mmap_ensured);
      }
    } else {
      // In this case, perform the image manipulation if we are dealing
      // with a shaped surface.
      if (host->contents_shaped)
        sl_xshape_generate_argb_image(
            host->ctx, &host->contents_shape, host->contents_shm_mmap,
            host->current_buffer->shape_image, host->contents_shm_format);
    }
  }

  if (host->contents_shm_mmap) {
    double contents_scale_x, contents_scale_y;
    wl_fixed_t contents_offset_x, contents_offset_y;
    compute_buffer_scale_and_offset(host, viewport, &contents_scale_x,
                                    &contents_scale_y, &contents_offset_x,
                                    &contents_offset_y);

    if (host->current_buffer->mmap->begin_write)
      host->current_buffer->mmap->begin_write(host->current_buffer->mmap->fd,
                                              host->ctx);

    // Copy damaged regions (surface-relative coordinates).
    int n;
    pixman_box32_t* rect =
        pixman_region32_rectangles(&host->current_buffer->surface_damage, &n);
    while (n--) {
      TRACE_EVENT("surface",
                  "sl_host_surface_commit: memcpy_loop (surface damage)");
      copy_damaged_rect(host, rect, host->contents_shaped, contents_scale_x,
                        contents_scale_y, wl_fixed_to_double(contents_offset_x),
                        wl_fixed_to_double(contents_offset_y));
      ++rect;
    }

    // Copy damaged regions (buffer-relative coordinates).
    //
    // In theory, if we've accumulated both surface damage and buffer damage,
    // it might be more efficient to first transform and union the regions, so
    // that we won't ever copy the same pixel twice.
    // In practice, wl_surface::damage_buffer obsoletes wl_surface::damage, and
    // it doesn't seem worthwhile to optimize for the edge case in which an app
    // uses both in the same frame.
    rect = pixman_region32_rectangles(&host->current_buffer->buffer_damage, &n);
    while (n--) {
      TRACE_EVENT("surface",
                  "sl_host_surface_commit: memcpy_loop (buffer damage)");
      copy_damaged_rect(host, rect, host->contents_shaped, 1.0, 1.0, 0.0, 0.0);
      ++rect;
    }

    if (host->current_buffer->mmap->end_write)
      host->current_buffer->mmap->end_write(host->current_buffer->mmap->fd,
                                            host->ctx);

    pixman_region32_clear(&host->current_buffer->surface_damage);
    pixman_region32_clear(&host->current_buffer->buffer_damage);

    wl_list_remove(&host->current_buffer->link);
    wl_list_insert(&host->busy_buffers, &host->current_buffer->link);
  }

  if (host->contents_width && host->contents_height) {
    double scale = host->ctx->scale * host->contents_scale;

    if (host->viewport) {
      int width = host->contents_width;
      int height = host->contents_height;

      // We need to take the client's viewport into account while still
      // making sure our scale is accounted for.
      if (viewport) {
        if (viewport->src_x >= 0 && viewport->src_y >= 0 &&
            viewport->src_width >= 0 && viewport->src_height >= 0) {
          wp_viewport_set_source(host->viewport, viewport->src_x,
                                 viewport->src_y, viewport->src_width,
                                 viewport->src_height);

          // If the source rectangle is set and the destination size is not
          // set, then src_width and src_height should be integers, and the
          // surface size becomes the source rectangle size.
          width = wl_fixed_to_int(viewport->src_width);
          height = wl_fixed_to_int(viewport->src_height);
        }

        // Use destination size as surface size when set.
        if (viewport->dst_width >= 0 && viewport->dst_height >= 0) {
          width = viewport->dst_width;
          height = viewport->dst_height;
        }
      } else {
        // Reset the host viewport if client viewport does not exist
        // TODO(b/328699937): There may be a bug/race in this section with
        // another part of Sommelier, leading to buffer size mismatch in
        // fullscreen mode.
        wl_fixed_t negative_one = wl_fixed_from_int(-1);
        wp_viewport_set_source(host->viewport, negative_one, negative_one,
                               negative_one, negative_one);
      }

      int32_t vp_width = width;
      int32_t vp_height = height;
      // Consult with the transform function to see if the
      // viewport destination set is necessary
      if (sl_transform_viewport_scale(host->ctx, host, host->contents_scale,
                                      &vp_width, &vp_height)) {
        wp_viewport_shim()->set_destination(host->viewport, vp_width,
                                            vp_height);
        if (window) {
          window->viewport_width_realized = vp_width;
          window->viewport_height_realized = vp_height;
        }
      } else {
        wp_viewport_shim()->set_destination(host->viewport, -1, -1);
      }
    } else {
      wl_surface_set_buffer_scale(host->proxy, scale);
    }
  }

  // No need to defer client commits if surface has a role. E.g. is a cursor
  // or shell surface.
  if (host->has_role) {
    TRACE_EVENT("surface", "sl_host_surface_commit: wl_surface_commit",
                "resource_id", resource_id, "has_role", host->has_role);
    wl_surface_commit(host->proxy);

    // GTK determines the scale based on the output the surface has entered.
    // If the surface has not entered any output, then have it enter the
    // internal output. TODO(reveman): Remove this when surface-output tracking
    // has been implemented in Chrome.
    if (!host->has_output) {
      for (auto output : host->ctx->host_outputs) {
        if (output->internal) {
          wl_surface_send_enter(host->resource, output->resource);
          host->has_output = 1;
          break;
        }
      }
    }
  } else {
    TRACE_EVENT("surface", "sl_host_surface_commit: wl_surface_commit",
                "resource_id", resource_id, "has_role", host->has_role);
    // Commit if surface is associated with a window. Otherwise, defer
    // commit until window is created.
    struct sl_window* window;
    wl_list_for_each(window, &host->ctx->windows, link) {
      if (window->host_surface_id == try_wl_resource_get_id(resource)) {
        if (window->xdg_surface) {
          wl_surface_commit(host->proxy);
          if (host->contents_width && host->contents_height)
            window->realized = 1;
        }
        break;
      }
    }
  }

  // Log frame stats.
  if (host->ctx->frame_stats != nullptr) {
    // Try and find a matching window to identify steam game id and
    // activation status.
    // TODO(davidriley): This ideally isn't doing a linear search each frame.
    uint32_t steam_game_id = 0;
    bool activated = false;
    struct sl_window* window;
    wl_list_for_each(window, &host->ctx->windows, link) {
      if (window->host_surface_id == try_wl_resource_get_id(resource)) {
        steam_game_id = window->steam_game_id;
        activated = window->activated;
        break;
      }
    }

    host->ctx->frame_stats->AddFrame(resource_id, steam_game_id, activated);
  }

  if (host->contents_shm_mmap) {
    if (host->contents_shm_mmap->buffer_resource) {
      wl_buffer_send_release(host->contents_shm_mmap->buffer_resource);
    }
    sl_mmap_end_access(host->contents_shm_mmap);
    sl_mmap_unref(host->contents_shm_mmap);
    host->contents_shm_mmap = nullptr;
  }

  if (window && sl_window_is_containerized(window)) {
    // Force borderless windows to be fullscreen if
    // window->borderless_window_check - this value is set to true when
    // client sends NET_WM_STATE_REMOVE for ATOM_NET_WM_STATE_FULLSCREEN.
    if (!window->fullscreen && !window->decorated &&
        !window->compositor_fullscreen && window->maybe_promote_to_fullscreen &&
        !window->iconified && window->activated) {
      window->maybe_promote_to_fullscreen = false;
      // Requesting Exo to set the window to fullscreen will result in
      // toplevel_configure, which updates the states (and
      // compositor_fullscreen is set appropriately as per request).
      xdg_toplevel_set_fullscreen(window->xdg_toplevel, nullptr);
    }
  }
}

static void sl_host_surface_set_buffer_scale(struct wl_client* client,
                                             struct wl_resource* resource,
                                             int32_t scale) {
  struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(resource));

  host->contents_scale = scale;
}

static const struct wl_surface_interface sl_surface_implementation = {
    sl_host_surface_destroy,
    sl_host_surface_attach,
    sl_host_surface_damage,
    sl_host_surface_frame,
    ForwardRequest<wl_surface_set_opaque_region, AllowNullResource::kYes>,
    ForwardRequest<wl_surface_set_input_region, AllowNullResource::kYes>,
    sl_host_surface_commit,
    ForwardRequest<wl_surface_set_buffer_transform>,
    sl_host_surface_set_buffer_scale,
    sl_host_surface_damage_buffer};

static void sl_destroy_host_surface(struct wl_resource* resource) {
  TRACE_EVENT("surface", "sl_destroy_host_surface", "resource_id",
              try_wl_resource_get_id(resource));
  struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_resource_get_user_data(resource));
  struct sl_window *window, *surface_window = nullptr;
  struct sl_output_buffer* buffer;

  wl_list_for_each(window, &host->ctx->windows, link) {
    if (window->host_surface_id == try_wl_resource_get_id(resource)) {
      surface_window = window;
      break;
    }
  }

  if (surface_window) {
    surface_window->host_surface_id = 0;
    sl_window_update(surface_window);
  }

  if (host->contents_shm_mmap)
    sl_mmap_unref(host->contents_shm_mmap);

  while (!wl_list_empty(&host->released_buffers)) {
    buffer = wl_container_of(host->released_buffers.next, buffer, link);
    sl_output_buffer_destroy(buffer);
  }
  while (!wl_list_empty(&host->busy_buffers)) {
    buffer = wl_container_of(host->busy_buffers.next, buffer, link);
    sl_output_buffer_destroy(buffer);
  }
  while (!wl_list_empty(&host->contents_viewport))
    wl_list_remove(host->contents_viewport.next);

  if (host->viewport)
    wp_viewport_destroy(host->viewport);
  wl_surface_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  if (host->surface_sync) {
    zwp_linux_surface_synchronization_v1_destroy(host->surface_sync);
    host->surface_sync = nullptr;
  }

  pixman_region32_fini(&host->contents_shape);
  delete host;
}

static void sl_surface_enter(void* data,
                             struct wl_surface* surface,
                             struct wl_output* output) {
  TRACE_EVENT("surface", "sl_surface_enter");
  struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_surface_get_user_data(surface));
  struct sl_host_output* host_output =
      static_cast<sl_host_output*>(wl_output_get_user_data(output));

  wl_surface_send_enter(host->resource, host_output->resource);
  host->has_output = 1;
  host->output = host_output;
  sl_transform_reset_surface_scale(host->ctx, host);
  struct sl_window* window = nullptr;
  window = sl_context_lookup_window_for_surface(host->ctx, host->resource);
  if (window) {
    sl_transform_try_window_scale(host->ctx, host, window->width,
                                  window->height);
    sl_window_update(window);
  }
}

static void sl_surface_leave(void* data,
                             struct wl_surface* surface,
                             struct wl_output* output) {
  TRACE_EVENT("surface", "sl_surface_leave");
  struct sl_host_surface* host =
      static_cast<sl_host_surface*>(wl_surface_get_user_data(surface));
  struct sl_host_output* host_output =
      static_cast<sl_host_output*>(wl_output_get_user_data(output));

  wl_surface_send_leave(host->resource, host_output->resource);
  host->has_output = 0;
  host->output.Reset();
}

static const struct wl_surface_listener sl_surface_listener = {
    sl_surface_enter, sl_surface_leave};

static void sl_region_destroy(struct wl_client* client,
                              struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_region_add(struct wl_client* client,
                          struct wl_resource* resource,
                          int32_t x,
                          int32_t y,
                          int32_t width,
                          int32_t height) {
  struct sl_host_region* host =
      static_cast<sl_host_region*>(wl_resource_get_user_data(resource));
  int32_t x1 = x;
  int32_t y1 = y;
  int32_t x2 = x + width;
  int32_t y2 = y + height;

  // A region object isn't attached to a surface, so
  // we will not use any surface specific scaling factors here
  //
  // TODO(mrisaacb): See if the effect of applying surface specific
  // scaling values to region objects is noticeable. Interjecting
  // here would require caching the object within Sommelier and
  // transferring the modified coordinates over when referenced.

  sl_transform_guest_to_host(host->ctx, nullptr, &x1, &y1);
  sl_transform_guest_to_host(host->ctx, nullptr, &x2, &y2);

  wl_region_add(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void sl_region_subtract(struct wl_client* client,
                               struct wl_resource* resource,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
  struct sl_host_region* host =
      static_cast<sl_host_region*>(wl_resource_get_user_data(resource));
  int32_t x1 = x;
  int32_t y1 = y;
  int32_t x2 = x + width;
  int32_t y2 = y + height;

  sl_transform_guest_to_host(host->ctx, nullptr, &x1, &y1);
  sl_transform_guest_to_host(host->ctx, nullptr, &x2, &y2);

  wl_region_subtract(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static const struct wl_region_interface sl_region_implementation = {
    sl_region_destroy, sl_region_add, sl_region_subtract};

static void sl_destroy_host_region(struct wl_resource* resource) {
  struct sl_host_region* host =
      static_cast<sl_host_region*>(wl_resource_get_user_data(resource));

  wl_region_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_compositor_create_host_surface(struct wl_client* client,
                                              struct wl_resource* resource,
                                              uint32_t id) {
  TRACE_EVENT("surface", "sl_compositor_create_host_surface");
  struct sl_host_compositor* host =
      static_cast<sl_host_compositor*>(wl_resource_get_user_data(resource));
  struct sl_window *window, *unpaired_window = nullptr;
  struct sl_host_surface* host_surface = new sl_host_surface();

  host_surface->ctx = host->compositor->ctx;
  host_surface->contents_width = 0;
  host_surface->contents_height = 0;
  host_surface->contents_x_offset = 0;
  host_surface->contents_y_offset = 0;
  host_surface->contents_shm_format = 0;
  host_surface->contents_scale = 1;
  wl_list_init(&host_surface->contents_viewport);
  host_surface->contents_shm_mmap = nullptr;
  host_surface->has_role = 0;
  host_surface->has_output = 0;
  host_surface->has_own_scale = 0;
  host_surface->xdg_scale_x = 0;
  host_surface->xdg_scale_y = 0;
  host_surface->scale_round_on_x = false;
  host_surface->scale_round_on_y = false;
  host_surface->last_event_serial = 0;
  host_surface->current_buffer = nullptr;
  host_surface->proxy_buffer = nullptr;
  host_surface->contents_shaped = false;
  host_surface->output = nullptr;
  pixman_region32_init(&host_surface->contents_shape);
  wl_list_init(&host_surface->released_buffers);
  wl_list_init(&host_surface->busy_buffers);
  host_surface->resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_surface->resource,
                                 &sl_surface_implementation, host_surface,
                                 sl_destroy_host_surface);
  host_surface->proxy = wl_compositor_create_surface(host->proxy);
  wl_surface_add_listener(host_surface->proxy, &sl_surface_listener,
                          host_surface);
  if (host_surface->ctx->linux_explicit_synchronization &&
      host_surface->ctx->use_explicit_fence) {
    host_surface->surface_sync =
        zwp_linux_explicit_synchronization_v1_get_synchronization(
            host_surface->ctx->linux_explicit_synchronization->internal,
            host_surface->proxy);
  } else {
    host_surface->surface_sync = nullptr;
  }
  host_surface->viewport = nullptr;
  if (host_surface->ctx->viewporter) {
    host_surface->viewport = wp_viewporter_get_viewport(
        host_surface->ctx->viewporter->internal, host_surface->proxy);
  }

  wl_list_for_each(window, &host->compositor->ctx->unpaired_windows, link) {
    if (window->host_surface_id == id) {
      unpaired_window = window;
      break;
    }
  }

  if (unpaired_window)
    sl_window_update(window);
}

static void sl_compositor_create_host_region(struct wl_client* client,
                                             struct wl_resource* resource,
                                             uint32_t id) {
  struct sl_host_compositor* host =
      static_cast<sl_host_compositor*>(wl_resource_get_user_data(resource));
  struct sl_host_region* host_region = new sl_host_region();

  host_region->ctx = host->compositor->ctx;
  host_region->resource = wl_resource_create(
      client, &wl_region_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_region->resource,
                                 &sl_region_implementation, host_region,
                                 sl_destroy_host_region);
  host_region->proxy = wl_compositor_create_region(host->proxy);
  wl_region_set_user_data(host_region->proxy, host_region);
}

static const struct wl_compositor_interface sl_compositor_implementation = {
    sl_compositor_create_host_surface, sl_compositor_create_host_region};

static void sl_destroy_host_compositor(struct wl_resource* resource) {
  struct sl_host_compositor* host =
      static_cast<sl_host_compositor*>(wl_resource_get_user_data(resource));

  wl_compositor_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

// Called when a Wayland client binds to our wl_compositor global.
// `version` is the version requested by the client.
static void sl_bind_host_compositor(struct wl_client* client,
                                    void* data,
                                    uint32_t version,
                                    uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_compositor* host = new sl_host_compositor();
  host->compositor = ctx->compositor;

  // Create the client-facing wl_compositor resource using the requested
  // version (or Sommelier's max supported version, whichever is lower).
  //
  // Sommelier requires a host compositor with wl_compositor version 3+,
  // but exposes wl_compositor v4 to its clients (if --support-damage-buffer
  // is passed). This is achieved by implementing wl_surface::damage_buffer (the
  // only v4 feature) in terms of the existing wl_surface::damage request.
  uint32_t maxSupportedVersion = ctx->support_damage_buffer
                                     ? WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION
                                     : kMinHostWlCompositorVersion;
  host->resource = wl_resource_create(client, &wl_compositor_interface,
                                      MIN(version, maxSupportedVersion), id);
  wl_resource_set_implementation(host->resource, &sl_compositor_implementation,
                                 host, sl_destroy_host_compositor);

  // Forward the bind request to the host, using the host's wl_compositor
  // version (which may be different from Sommelier's version).
  host->proxy = static_cast<wl_compositor*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), ctx->compositor->id,
      &wl_compositor_interface, kMinHostWlCompositorVersion));
  wl_compositor_set_user_data(host->proxy, host);
}

struct sl_global* sl_compositor_global_create(struct sl_context* ctx) {
  assert(ctx->compositor);
  // Compute the compositor version to advertise to clients, depending on the
  // --support-damage-buffer flag (see explanation above).
  int compositorVersion = ctx->support_damage_buffer
                              ? WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION
                              : kMinHostWlCompositorVersion;
  return sl_global_create(ctx, &wl_compositor_interface, compositorVersion, ctx,
                          sl_bind_host_compositor);
}

void sl_compositor_init_context(struct sl_context* ctx,
                                struct wl_registry* registry,
                                uint32_t id,
                                uint32_t version) {
  struct sl_compositor* compositor =
      static_cast<sl_compositor*>(malloc(sizeof(struct sl_compositor)));
  assert(compositor);
  compositor->ctx = ctx;
  compositor->id = id;
  assert(version >= kMinHostWlCompositorVersion);
  compositor->internal = static_cast<wl_compositor*>(wl_registry_bind(
      registry, id, &wl_compositor_interface, kMinHostWlCompositorVersion));
  assert(!ctx->compositor);
  ctx->compositor = compositor;
  compositor->host_global = sl_compositor_global_create(ctx);
}
