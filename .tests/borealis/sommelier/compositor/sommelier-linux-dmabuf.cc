// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gbm.h>
#include <libdrm/drm_fourcc.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <wayland-server-protocol.h>
#include <xf86drm.h>

#include "../sommelier.h"         // NOLINT(build/include_directory)
#include "../sommelier-global.h"  // NOLINT(build/include_directory)
#include "../virtualization/linux-headers/virtgpu_drm.h"  // NOLINT(build/include_directory)
#include "sommelier-dmabuf-sync.h"   // NOLINT(build/include_directory)
#include "sommelier-formats.h"       // NOLINT(build/include_directory)
#include "sommelier-linux-dmabuf.h"  // NOLINT(build/include_directory)

#include "linux-dmabuf-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)
#include "linux-dmabuf-unstable-v1-server-protocol.h"  // NOLINT(build/include_directory)

#if WITH_TESTS
#include "sommelier-linux-dmabuf-test.h"  // NOLINT(build/include_directory)

static struct linux_dmabuf_test_fixture linux_dmabuf_fixture;

struct linux_dmabuf_test_fixture get_linux_dmabuf_test_fixture() {
  return linux_dmabuf_fixture;
}
#endif

struct sl_linux_dmabuf_format {
  uint32_t format;
};

struct sl_linux_dmabuf_modifier {
  uint32_t format;
  uint32_t modifier_hi;
  uint32_t modifier_lo;
};

struct sl_host_linux_dmabuf {
  sl_linux_dmabuf* linux_dmabuf;
  struct wl_resource* resource;
  struct zwp_linux_dmabuf_v1* proxy;
  uint32_t version;

  std::vector<sl_linux_dmabuf_format> formats;
  std::vector<sl_linux_dmabuf_modifier> modifiers;
};

struct sl_linux_dmabuf_packed_format {
  uint32_t format;
  uint32_t _padding;
  uint64_t modifier;
};

struct sl_host_linux_dmabuf_feedback {
  sl_host_linux_dmabuf* host_linux_dmabuf;
  struct wl_resource* resource;
  struct zwp_linux_dmabuf_feedback_v1* proxy;

  uint32_t map_size;
  struct sl_linux_dmabuf_packed_format* mapped_format_table;
};

struct sl_host_linux_buffer_params {
  sl_host_linux_dmabuf* host_linux_dmabuf;
  struct wl_resource* resource;
  struct zwp_linux_buffer_params_v1* proxy;

  uint32_t width;
  uint32_t height;
  uint32_t format;

  bool is_virtgpu_buffer;
  struct {
    uint32_t stride;
    int dmabuf_fd;
  } plane0;
};

struct sl_host_buffer* sl_linux_dmabuf_create_host_buffer(
    struct sl_context* ctx,
    struct wl_client* client,
    struct wl_buffer* buffer_proxy,
    uint32_t buffer_id,
    const struct sl_linux_dmabuf_host_buffer_create_info* create_info) {
  assert(sl_drm_format_is_supported(create_info->format));

  struct sl_host_buffer* host_buffer =
      sl_create_host_buffer(ctx, client, buffer_id, buffer_proxy,
                            create_info->width, create_info->height, true);

  if (create_info->is_virtgpu_buffer) {
    host_buffer->sync_point = sl_sync_point_create(create_info->dmabuf_fd);
    host_buffer->sync_point->sync = sl_dmabuf_sync;
    host_buffer->shm_format =
        sl_shm_format_from_drm_format(create_info->format);

    // Create our DRM PRIME mmap container
    // This is simply a container that records necessary information
    // to map the DRM buffer through the GBM API's.
    // The GBM API's may need to perform a rather heavy copy of the
    // buffer into memory accessible by the CPU to perform the mapping
    // operation.
    // For this reason, the GBM mapping API's will not be used until we
    // are absolutely certain that the buffers contents need to be
    // accessed. This will be done through a call to sl_mmap_begin_access.
    //
    // We are also checking for a single plane format as this container
    // is currently only defined for single plane format buffers.

    if (sl_shm_format_num_planes(host_buffer->shm_format) == 1) {
      host_buffer->shm_mmap = sl_drm_prime_mmap_create(
          ctx->gbm, create_info->dmabuf_fd,
          sl_shm_format_bpp(host_buffer->shm_format),
          sl_shm_format_num_planes(host_buffer->shm_format),
          create_info->stride, create_info->width, create_info->height,
          create_info->format);

      // The buffer_resource must be set appropriately here or else
      // we will not perform the appropriate release at the end of
      // sl_host_surface_commit (see the end of that function for details).
      //
      // This release should only be done IF we successfully perform
      // the xshape interjection, as the host compositor will be using
      // a different buffer. For non shaped windows or fallbacks due
      // to map failure, where the buffer is relayed onto the host,
      // we should not release the buffer. That is the responsibility
      // of the host. The fallback path/non shape path takes care of this
      // by setting the host_surface contents_shm_mmap to nullptr.
      host_buffer->shm_mmap->buffer_resource = host_buffer->resource;
    }
  } else if (create_info->dmabuf_fd >= 0) {
    close(create_info->dmabuf_fd);
  }

  return host_buffer;
}

bool sl_linux_dmabuf_fixup_plane0_params(gbm_device* gbm,
                                         int32_t fd,
                                         uint32_t* out_stride,
                                         uint32_t* out_modifier_hi,
                                         uint32_t* out_modifier_lo) {
  /* silently perform fixups for virtgpu classic resources that were created
   * with implicit modifiers (resolved to explicit modifier in host minigbm)
   * and may have different stride for host buffer and shadow/guest buffer.
   * For context, see: crbug.com/892242 and b/230510320.
   */
  int drm_fd = gbm_device_get_fd(gbm);
  struct drm_prime_handle prime_handle;
  int ret;
  bool is_virtgpu_buffer = false;

  // First imports the prime fd to a gem handle. This will fail if this
  // function was not passed a prime handle that can be imported by the drm
  // device given to sommelier.
  memset(&prime_handle, 0, sizeof(prime_handle));
  prime_handle.fd = fd;
  ret = drmIoctl(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle);
  if (!ret) {
    struct drm_virtgpu_resource_info_cros info_arg;
    struct drm_gem_close gem_close;

    // Then attempts to get resource information. This will fail silently if
    // the drm device passed to sommelier is not a virtio-gpu device.
    memset(&info_arg, 0, sizeof(info_arg));
    info_arg.bo_handle = prime_handle.handle;
    info_arg.type = VIRTGPU_RESOURCE_INFO_TYPE_EXTENDED;
    ret = drmIoctl(drm_fd, DRM_IOCTL_VIRTGPU_RESOURCE_INFO_CROS, &info_arg);
    // Correct stride if we are able to get proper resource info.
    if (!ret) {
      is_virtgpu_buffer = true;
      if (info_arg.stride) {
        *out_stride = info_arg.stride;
        *out_modifier_hi = info_arg.format_modifier >> 32;
        *out_modifier_lo = info_arg.format_modifier & 0xFFFFFFFF;
      }
    }

    // Always close the handle we imported.
    memset(&gem_close, 0, sizeof(gem_close));
    gem_close.handle = prime_handle.handle;
    drmIoctl(drm_fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
  }

  return is_virtgpu_buffer;
}

/*
 * LISTENER: zwp_linux_dmabuf_feedback_v1
 */
static void sl_linux_dmabuf_feedback_done(
    void* data, struct zwp_linux_dmabuf_feedback_v1* feedback) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(data);

  zwp_linux_dmabuf_feedback_v1_send_done(host_feedback->resource);
}

static void sl_linux_dmabuf_feedback_format_table(
    void* data,
    struct zwp_linux_dmabuf_feedback_v1* feedback,
    int32_t fd,
    uint32_t size) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(data);
  struct sl_context* ctx = host_feedback->host_linux_dmabuf->linux_dmabuf->ctx;

  void* map_addr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map_addr == MAP_FAILED) {
    wl_client_post_implementation_error(
        ctx->client, "failed to map format table with error %d", errno);
  } else {
    host_feedback->map_size = size;
    host_feedback->mapped_format_table =
        static_cast<struct sl_linux_dmabuf_packed_format*>(map_addr);

    zwp_linux_dmabuf_feedback_v1_send_format_table(host_feedback->resource, fd,
                                                   size);
  }

  close(fd);
}

/* returns 'device' unmodified or 'local_device' initialized with the gbm
 * device's dev_id.
 */
static wl_array* fixup_drm_device(sl_context* ctx,
                                  wl_array* device,
                                  wl_array* local_device) {
  if (ctx->gbm) {
    dev_t dev;
    struct stat buf;

    assert(device->size == sizeof(dev));
    memcpy(&dev, device->data, sizeof(dev));

    int gbm_fd = gbm_device_get_fd(ctx->gbm);
    if (gbm_fd < 0 || fstat(gbm_fd, &buf) == -1) {
      wl_client_post_implementation_error(ctx->client, "gbm device lost");
      return nullptr;
    }

    wl_array_init(local_device);
    void* to = wl_array_add(local_device, sizeof(buf.st_rdev));
    if (!to) {
      wl_array_release(local_device);
      wl_client_post_no_memory(ctx->client);
      return nullptr;
    }

    memcpy(to, &buf.st_rdev, sizeof(buf.st_rdev));
    device = local_device;
  }

  return device;
}

static void sl_linux_dmabuf_feedback_main_device(
    void* data,
    struct zwp_linux_dmabuf_feedback_v1* feedback,
    struct wl_array* device) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(data);
  struct sl_context* ctx = host_feedback->host_linux_dmabuf->linux_dmabuf->ctx;

  wl_array local_device;
  device = fixup_drm_device(ctx, device, &local_device);
  if (!device) {
    return;
  }

  zwp_linux_dmabuf_feedback_v1_send_main_device(host_feedback->resource,
                                                device);

  if (device == &local_device) {
    wl_array_release(&local_device);
  }
}

static void sl_linux_dmabuf_feedback_tranche_done(
    void* data, struct zwp_linux_dmabuf_feedback_v1* feedback) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(data);

  zwp_linux_dmabuf_feedback_v1_send_tranche_done(host_feedback->resource);
}

static void sl_linux_dmabuf_feedback_tranche_target_device(
    void* data,
    struct zwp_linux_dmabuf_feedback_v1* feedback,
    struct wl_array* device) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(data);
  struct sl_context* ctx = host_feedback->host_linux_dmabuf->linux_dmabuf->ctx;

  wl_array local_device;
  device = fixup_drm_device(ctx, device, &local_device);
  if (!device) {
    return;
  }

  zwp_linux_dmabuf_feedback_v1_send_tranche_target_device(
      host_feedback->resource, device);

  if (device == &local_device) {
    wl_array_release(&local_device);
  }
}

static void sl_linux_dmabuf_feedback_tranche_formats(
    void* data,
    struct zwp_linux_dmabuf_feedback_v1* feedback,
    struct wl_array* indices) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(data);
  struct sl_context* ctx = host_feedback->host_linux_dmabuf->linux_dmabuf->ctx;

  wl_array filtered;
  wl_array_init(&filtered);

  // TODO(b/309012293): format filtering added to match wl_drm behavior while
  // sommelier+virgl depend on a known list of supported formats.
  uint16_t* index;
  sl_array_for_each(index, indices) {
    const sl_linux_dmabuf_packed_format* packed =
        &host_feedback->mapped_format_table[*index];
    if (sl_drm_format_is_supported(packed->format)) {
      uint16_t* dst =
          static_cast<uint16_t*>(wl_array_add(&filtered, sizeof(uint16_t)));
      if (!dst) {
        wl_array_release(&filtered);
        wl_client_post_no_memory(ctx->client);
        return;
      }
      *dst = *index;
    }
  }

  zwp_linux_dmabuf_feedback_v1_send_tranche_formats(host_feedback->resource,
                                                    &filtered);
  wl_array_release(&filtered);
}

static void sl_linux_dmabuf_feedback_tranche_flags(
    void* data, struct zwp_linux_dmabuf_feedback_v1* feedback, uint32_t flags) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(data);

  zwp_linux_dmabuf_feedback_v1_send_tranche_flags(host_feedback->resource,
                                                  flags);
}

static struct zwp_linux_dmabuf_feedback_v1_listener
    sl_linux_dmabuf_feedback_listener = {
        .done = sl_linux_dmabuf_feedback_done,
        .format_table = sl_linux_dmabuf_feedback_format_table,
        .main_device = sl_linux_dmabuf_feedback_main_device,
        .tranche_done = sl_linux_dmabuf_feedback_tranche_done,
        .tranche_target_device = sl_linux_dmabuf_feedback_tranche_target_device,
        .tranche_formats = sl_linux_dmabuf_feedback_tranche_formats,
        .tranche_flags = sl_linux_dmabuf_feedback_tranche_flags,
};

/*
 * IMPLEMENTATION: zwp_linux_dmabuf_feedback_v1
 */
static void sl_linux_dmabuf_feedback_resource_destroy(
    struct wl_resource* resource) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      static_cast<struct sl_host_linux_dmabuf_feedback*>(
          wl_resource_get_user_data(resource));

  if (host_feedback->mapped_format_table) {
    int ret =
        munmap(host_feedback->mapped_format_table, host_feedback->map_size);
    assert(!ret);
    UNUSED(ret);
  }

  zwp_linux_dmabuf_feedback_v1_destroy(host_feedback->proxy);
  delete host_feedback;
}

static void sl_linux_dmabuf_feedback_destroy(struct wl_client* client,
                                             struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static struct zwp_linux_dmabuf_feedback_v1_interface
    sl_linux_dmabuf_feedback_implementation = {
        .destroy = sl_linux_dmabuf_feedback_destroy,
};

/*
 * LISTENER: zwp_linux_buffer_params_v1
 */
static void sl_linux_buffer_params_created(
    void* data,
    struct zwp_linux_buffer_params_v1* zwp_linux_buffer_params_v1,
    struct wl_buffer* buffer_proxy) {
  struct sl_host_linux_buffer_params* host_params =
      static_cast<struct sl_host_linux_buffer_params*>(data);

  struct sl_context* ctx = host_params->host_linux_dmabuf->linux_dmabuf->ctx;
  struct wl_client* client = wl_resource_get_client(host_params->resource);

  const struct sl_linux_dmabuf_host_buffer_create_info create_info = {
      .width = host_params->width,
      .height = host_params->height,
      .stride = host_params->plane0.stride,
      .format = host_params->format,
      .dmabuf_fd = host_params->plane0.dmabuf_fd,
      .is_virtgpu_buffer = host_params->is_virtgpu_buffer,
  };
  // dmabuf_fd ownership is transferred to callee
  struct sl_host_buffer* host_buffer = sl_linux_dmabuf_create_host_buffer(
      ctx, client, buffer_proxy, 0, &create_info);
  host_params->plane0.dmabuf_fd = -1;

  zwp_linux_buffer_params_v1_send_created(host_params->resource,
                                          host_buffer->resource);
}

static void sl_linux_buffer_params_failed(
    void* data, struct zwp_linux_buffer_params_v1* zwp_linux_buffer_params_v1) {
  struct sl_host_linux_buffer_params* host_params =
      static_cast<struct sl_host_linux_buffer_params*>(data);

  zwp_linux_buffer_params_v1_send_failed(host_params->resource);
}

static struct zwp_linux_buffer_params_v1_listener
    sl_linux_buffer_params_listener = {
        .created = sl_linux_buffer_params_created,
        .failed = sl_linux_buffer_params_failed,
};

/*
 * IMPLEMENTATION: zwp_linux_buffer_params_v1
 */
static void sl_linux_buffer_params_resource_destroy(
    struct wl_resource* resource) {
  struct sl_host_linux_buffer_params* host_params =
      static_cast<struct sl_host_linux_buffer_params*>(
          wl_resource_get_user_data(resource));

  zwp_linux_buffer_params_v1_destroy(host_params->proxy);

  if (host_params->plane0.dmabuf_fd >= 0) {
    close(host_params->plane0.dmabuf_fd);
  }
  delete host_params;
}

static void sl_linux_buffer_params_destroy(struct wl_client* client,
                                           struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_linux_buffer_params_add(struct wl_client* client,
                                       struct wl_resource* resource,
                                       int32_t fd,
                                       uint32_t plane_idx,
                                       uint32_t offset,
                                       uint32_t stride,
                                       uint32_t modifier_hi,
                                       uint32_t modifier_lo) {
  struct sl_host_linux_buffer_params* host_params =
      static_cast<struct sl_host_linux_buffer_params*>(
          wl_resource_get_user_data(resource));
  struct sl_context* ctx = host_params->host_linux_dmabuf->linux_dmabuf->ctx;

  if (ctx->gbm && plane_idx == 0 && fd >= 0) {
    if (host_params->plane0.dmabuf_fd >= 0) {
      wl_resource_post_error(resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                             "dmabuf already set for plane 0");
      close(fd);
      return;
    }

    /* query the host buffer metadata if the fd belongs to a virtio-gpu classic
     * 3d resource (virgl), or silently leave unmodified.
     */
    bool is_virtgpu_buffer = sl_linux_dmabuf_fixup_plane0_params(
        ctx->gbm, fd, &stride, &modifier_hi, &modifier_lo);

    if (stride == 0) {
      wl_resource_post_error(resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                             "invalid stride 0");
      close(fd);
      return;
    }

    host_params->plane0.stride = stride;
    host_params->plane0.dmabuf_fd = dup(fd);
    host_params->is_virtgpu_buffer = is_virtgpu_buffer;
  }

  zwp_linux_buffer_params_v1_add(host_params->proxy, fd, plane_idx, offset,
                                 stride, modifier_hi, modifier_lo);
  if (fd >= 0) {
    close(fd);
  }
}

static void sl_linux_buffer_params_create(struct wl_client* client,
                                          struct wl_resource* resource,
                                          int32_t width,
                                          int32_t height,
                                          uint32_t format,
                                          uint32_t flags) {
  struct sl_host_linux_buffer_params* host_params =
      static_cast<struct sl_host_linux_buffer_params*>(
          wl_resource_get_user_data(resource));

  if (!sl_drm_format_is_supported(format)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "invalid format %u", format);
    return;
  }

  host_params->width = width;
  host_params->height = height;
  host_params->format = format;
  zwp_linux_buffer_params_v1_create(host_params->proxy, width, height, format,
                                    flags);
}

static void sl_linux_buffer_params_create_immed(struct wl_client* client,
                                                struct wl_resource* resource,
                                                uint32_t buffer_id,
                                                int32_t width,
                                                int32_t height,
                                                uint32_t format,
                                                uint32_t flags) {
  struct sl_host_linux_buffer_params* host_params =
      static_cast<struct sl_host_linux_buffer_params*>(
          wl_resource_get_user_data(resource));
  struct sl_context* ctx = host_params->host_linux_dmabuf->linux_dmabuf->ctx;

  if (!sl_drm_format_is_supported(format)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "invalid format %u", format);
    return;
  }

  struct wl_buffer* buffer_proxy = zwp_linux_buffer_params_v1_create_immed(
      host_params->proxy, width, height, format, flags);

  const struct sl_linux_dmabuf_host_buffer_create_info info = {
      .width = static_cast<uint32_t>(width),
      .height = static_cast<uint32_t>(height),
      .stride = host_params->plane0.stride,
      .format = format,
      .dmabuf_fd = host_params->plane0.dmabuf_fd,
      .is_virtgpu_buffer = host_params->is_virtgpu_buffer,
  };
  // dmabuf_fd ownership is transferred to callee
  sl_linux_dmabuf_create_host_buffer(ctx, client, buffer_proxy, buffer_id,
                                     &info);
  host_params->plane0.dmabuf_fd = -1;
}

static struct zwp_linux_buffer_params_v1_interface
    sl_linux_buffer_params_implementation = {
        .destroy = sl_linux_buffer_params_destroy,
        .add = sl_linux_buffer_params_add,
        .create = sl_linux_buffer_params_create,
        .create_immed = sl_linux_buffer_params_create_immed,
};

/*
 * IMPLEMENTATION: zwp_linux_dmabuf_v1
 */
static void sl_linux_dmabuf_resource_destroy(struct wl_resource* resource) {
  struct sl_host_linux_dmabuf* host_linux_dmabuf =
      static_cast<struct sl_host_linux_dmabuf*>(
          wl_resource_get_user_data(resource));

  if (host_linux_dmabuf->version <
      ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION) {
    host_linux_dmabuf->formats.clear();
    if (host_linux_dmabuf->version >=
        ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
      host_linux_dmabuf->modifiers.clear();
    }
  }
  zwp_linux_dmabuf_v1_destroy(host_linux_dmabuf->proxy);
  delete host_linux_dmabuf;
}

static void sl_linux_dmabuf_destroy(struct wl_client* client,
                                    struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_linux_dmabuf_create_params(struct wl_client* client,
                                          struct wl_resource* resource,
                                          uint32_t id) {
  struct sl_host_linux_dmabuf* host_linux_dmabuf =
      static_cast<sl_host_linux_dmabuf*>(wl_resource_get_user_data(resource));

  struct sl_host_linux_buffer_params* host_params =
      new sl_host_linux_buffer_params{};
  host_params->host_linux_dmabuf = host_linux_dmabuf;
  host_params->plane0.dmabuf_fd = -1;

  host_params->proxy =
      zwp_linux_dmabuf_v1_create_params(host_linux_dmabuf->proxy);
  zwp_linux_buffer_params_v1_add_listener(host_params->proxy,
                                          &sl_linux_buffer_params_listener,
                                          static_cast<void*>(host_params));

  host_params->resource =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                         host_linux_dmabuf->version, id);
  wl_resource_set_implementation(
      host_params->resource, &sl_linux_buffer_params_implementation,
      static_cast<void*>(host_params), sl_linux_buffer_params_resource_destroy);
}

static struct sl_host_linux_dmabuf_feedback* sl_create_host_feedback(
    wl_client* client,
    uint32_t id,
    struct sl_host_linux_dmabuf* host_linux_dmabuf,
    zwp_linux_dmabuf_feedback_v1* proxy) {
  struct sl_host_linux_dmabuf_feedback* host_feedback =
      new sl_host_linux_dmabuf_feedback{};
  host_feedback->host_linux_dmabuf = host_linux_dmabuf;
  host_feedback->proxy = proxy;

  zwp_linux_dmabuf_feedback_v1_add_listener(proxy,
                                            &sl_linux_dmabuf_feedback_listener,
                                            static_cast<void*>(host_feedback));

  host_feedback->resource =
      wl_resource_create(client, &zwp_linux_dmabuf_feedback_v1_interface,
                         host_linux_dmabuf->version, id);
  wl_resource_set_implementation(host_feedback->resource,
                                 &sl_linux_dmabuf_feedback_implementation,
                                 static_cast<void*>(host_feedback),
                                 sl_linux_dmabuf_feedback_resource_destroy);

  return host_feedback;
}

static void sl_linux_dmabuf_get_default_feedback(struct wl_client* client,
                                                 struct wl_resource* resource,
                                                 uint32_t id) {
  struct sl_host_linux_dmabuf* host_linux_dmabuf =
      static_cast<sl_host_linux_dmabuf*>(wl_resource_get_user_data(resource));

  zwp_linux_dmabuf_feedback_v1* proxy =
      zwp_linux_dmabuf_v1_get_default_feedback(host_linux_dmabuf->proxy);

  sl_create_host_feedback(client, id, host_linux_dmabuf, proxy);
}

static void sl_linux_dmabuf_get_surface_feedback(struct wl_client* client,
                                                 struct wl_resource* resource,
                                                 uint32_t id,
                                                 struct wl_resource* surface) {
  struct sl_host_linux_dmabuf* host_linux_dmabuf =
      static_cast<sl_host_linux_dmabuf*>(wl_resource_get_user_data(resource));
  struct sl_host_surface* host_surface =
      static_cast<struct sl_host_surface*>(wl_resource_get_user_data(surface));

  zwp_linux_dmabuf_feedback_v1* proxy =
      zwp_linux_dmabuf_v1_get_surface_feedback(host_linux_dmabuf->proxy,
                                               host_surface->proxy);

  sl_create_host_feedback(client, id, host_linux_dmabuf, proxy);
}

static struct zwp_linux_dmabuf_v1_interface sl_linux_dmabuf_implementation {
  .destroy = sl_linux_dmabuf_destroy,
  .create_params = sl_linux_dmabuf_create_params,
  .get_default_feedback = sl_linux_dmabuf_get_default_feedback,
  .get_surface_feedback = sl_linux_dmabuf_get_surface_feedback,
};

/*
 * LISTENER: zwp_linux_dmabuf_v1
 */
static void sl_linux_dmabuf_format(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
    uint32_t format) {
  struct sl_host_linux_dmabuf* host_linux_dmabuf =
      static_cast<sl_host_linux_dmabuf*>(data);

  assert(host_linux_dmabuf->version >=
         ZWP_LINUX_DMABUF_V1_FORMAT_SINCE_VERSION);
  assert(host_linux_dmabuf->version <
         ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION);

  if (sl_drm_format_is_supported(format)) {
    const struct sl_linux_dmabuf_format fmt = {
        .format = format,
    };
    host_linux_dmabuf->formats.push_back(fmt);
  }
}

static void sl_linux_dmabuf_modifier(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
    uint32_t format,
    uint32_t modifier_hi,
    uint32_t modifier_lo) {
  struct sl_host_linux_dmabuf* host_linux_dmabuf =
      static_cast<sl_host_linux_dmabuf*>(data);
  assert(host_linux_dmabuf->version >=
         ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION);
  assert(host_linux_dmabuf->version <
         ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION);

  if (sl_drm_format_is_supported(format)) {
    const struct sl_linux_dmabuf_modifier mod = {
        .format = format,
        .modifier_hi = modifier_hi,
        .modifier_lo = modifier_lo,
    };
    host_linux_dmabuf->modifiers.push_back(mod);
  }
}

static struct zwp_linux_dmabuf_v1_listener sl_linux_dmabuf_listener = {
    .format = sl_linux_dmabuf_format,
    .modifier = sl_linux_dmabuf_modifier,
};

/*
 * GLOBAL: zwp_linux_dmabuf_v1
 */
static void sl_linux_dmabuf_bind_callback(void* data,
                                          struct wl_callback* wl_callback,
                                          uint32_t callback_data) {
  struct sl_host_linux_dmabuf* host_linux_dmabuf =
      static_cast<sl_host_linux_dmabuf*>(data);
  assert(host_linux_dmabuf->version <
         ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION);

  for (const auto& fmt : host_linux_dmabuf->formats) {
    zwp_linux_dmabuf_v1_send_format(host_linux_dmabuf->resource, fmt.format);
  }

  if (host_linux_dmabuf->version >=
      ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
    for (const auto& mod : host_linux_dmabuf->modifiers) {
      zwp_linux_dmabuf_v1_send_modifier(host_linux_dmabuf->resource, mod.format,
                                        mod.modifier_hi, mod.modifier_lo);
    }
  }
}

static struct wl_callback_listener sl_linux_dmabuf_bind_callback_listener = {
    .done = sl_linux_dmabuf_bind_callback,
};

static void sl_bind_host_linux_dmabuf(struct wl_client* client,
                                      void* data,
                                      uint32_t version,
                                      uint32_t id) {
  struct sl_linux_dmabuf* linux_dmabuf =
      static_cast<struct sl_linux_dmabuf*>(data);
  struct sl_context* ctx = linux_dmabuf->ctx;

  struct sl_host_linux_dmabuf* host_linux_dmabuf = new sl_host_linux_dmabuf{};
  host_linux_dmabuf->linux_dmabuf = linux_dmabuf;

  host_linux_dmabuf->resource =
      wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, version, id);
  wl_resource_set_implementation(
      host_linux_dmabuf->resource, &sl_linux_dmabuf_implementation,
      static_cast<void*>(host_linux_dmabuf), sl_linux_dmabuf_resource_destroy);

  struct wl_registry* registry = wl_display_get_registry(ctx->display);
  host_linux_dmabuf->proxy = static_cast<zwp_linux_dmabuf_v1*>(wl_registry_bind(
      registry, linux_dmabuf->id, &zwp_linux_dmabuf_v1_interface, version));
  host_linux_dmabuf->version = version;

  // Send format/modifier events when ready if bound with version <=3.
  // This behavior is deprecated starting in protocol version 4.
  if (version < ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION) {
    zwp_linux_dmabuf_v1_add_listener(host_linux_dmabuf->proxy,
                                     &sl_linux_dmabuf_listener,
                                     static_cast<void*>(host_linux_dmabuf));

    /* The following callback is guaranteed to complete before the client's next
     * wl_display_sync() (typically placed after it issues global bindings):
     *
     *  1. Client requests global binding (request 'Ac') and follows with
     *     wl_display_sync(), creating callback (request 'Bc') and setting a
     *     listener for 'Bc::done'.
     *  2. (Client flushes to Sommelier)
     *  3. Sommelier dispatches this procedure to handle request 'Ac', which
     *     passes through (as 'As') and installs an internal callback request
     *     'Cs' to the server.
     *  4. Sommelier dispatches handler for 'Bc', which installs an internal
     *     callback request 'Bs' to the server and sets a listener for
     *     'Bs::done'.
     *  5. (Sommelier flushes to server)
     *  6. Server dispatches handler for 'As', sending format/modifier lists to
     *     Sommelier (denoted as 'As::formats').
     *  7. Server signals 'Cs::done', then 'Bs::done'
     *  8. (Server flushes to Sommelier)
     *  9. Sommelier handles 'As::formats', storing format/modifier lists.
     * 10. Sommelier's 'Cs::done' listener sends format/modifier lists to
     *     Client (denoted as 'Ac::formats').
     * 11. Sommelier's 'Bs::done' listener signals 'Bc::done'
     * 12. (Sommelier flushes to Client).
     * 13. Client receives format/modifier lists, then its wl_display_sync()
     *     unblocks upon 'Bc::done'.
     *
     * As a diagram, where Ac is a sent request/event and (Ac) is its
     * corresponding handler:
     *   Client:     Ac Bc ->
     *   Sommelier: (Ac) As Cs (Bc) Bs ->
     *   Server:    (As) As::formats (Cs) Cs::done (Bs) Bs::done ->
     *   Sommelier: (As::formats) (Cs::done) Ac::formats (Bs::done) Bc::done ->
     *   Client:    (Ac::formats) (Bc::done)
     *
     * The crucial guarantee is that (Ac::formats) occurs before (Bc::done), so
     * the client is guaranteed to have received all formats/modifiers before
     * proceeding.
     */
    wl_callback* cb = wl_display_sync(ctx->display);
    wl_callback_add_listener(cb, &sl_linux_dmabuf_bind_callback_listener,
                             static_cast<void*>(host_linux_dmabuf));
#if WITH_TESTS
    linux_dmabuf_fixture.bind_callback_proxy = cb;
#endif
  }

#if WITH_TESTS
  linux_dmabuf_fixture.proxy = host_linux_dmabuf->proxy;
  linux_dmabuf_fixture.user_data = static_cast<void*>(host_linux_dmabuf);
#endif
}

struct sl_global* sl_linux_dmabuf_global_create(
    struct sl_context* ctx, struct sl_linux_dmabuf* linux_dmabuf) {
  return sl_global_create(
      ctx, &zwp_linux_dmabuf_v1_interface, linux_dmabuf->version,
      static_cast<void*>(linux_dmabuf), sl_bind_host_linux_dmabuf);
}
