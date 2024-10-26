// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xf86drm.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../sommelier-logging.h"           // NOLINT(build/include_directory)
#include "linux-headers/virtgpu_drm.h"      // NOLINT(build/include_directory)
#include "virtgpu_cross_domain_protocol.h"  // NOLINT(build/include_directory)
#include "wayland_channel.h"                // NOLINT(build/include_directory)

// The size of a page for the guest kernel
#define PAGE_SIZE (getpagesize())

// We require six virtgpu params to use the virtgpu channel
#define REQUIRED_PARAMS_SIZE 6

// The capset for the virtgpu cross domain context type (defined internally
// for now)
#define CAPSET_CROSS_DOMAIN 5

// Constants taken from pipe_loader_drm.c in Mesa
#define DRM_NUM_NODES 63

// DRM Render nodes start at 128
#define DRM_RENDER_NODE_START 128

#define MAX_SEND_SIZE \
  (DEFAULT_BUFFER_SIZE - sizeof(struct CrossDomainSendReceive))
#define MAX_WRITE_SIZE \
  (DEFAULT_BUFFER_SIZE - sizeof(struct CrossDomainReadWrite))

struct virtgpu_param {
  uint64_t param;
  const char* name;
  uint32_t value;
};

#define PARAM(x)           \
  (struct virtgpu_param) { \
    x, #x, 0               \
  }

int open_virtgpu(char** drm_device) {
  int fd;
  char* node;
  drmVersionPtr drm_version;

  uint32_t num_nodes = DRM_NUM_NODES;
  uint32_t min_render_node = DRM_RENDER_NODE_START;
  uint32_t max_render_node = (min_render_node + num_nodes);

  for (uint32_t idx = min_render_node; idx < max_render_node; idx++) {
    if (asprintf(&node, "%s/renderD%d", DRM_DIR_NAME, idx) < 0)
      continue;

    fd = open(node, O_RDWR | O_CLOEXEC);

    if (fd < 0) {
      free(node);
      continue;
    }

    drm_version = drmGetVersion(fd);
    if (!drm_version) {
      free(node);
      close(fd);
      continue;
    }

    if (!strcmp(drm_version->name, "virtio_gpu")) {
      drmFreeVersion(drm_version);
      *drm_device = strdup(node);
      free(node);
      return fd;
    }

    drmFreeVersion(drm_version);
    free(node);
    close(fd);
  }

  return -1;
}

int32_t fstat_pipe(int fd, uint32_t& inode) {
  int32_t ret;
  struct stat statbuf = {};

  ret = fstat(fd, &statbuf);

  if (ret) {
    LOG(ERROR) << "fstat failed: " << strerror(errno);
    return ret;
  }

  // fstat + S_ISFIFO(..) will return true for both anonymous and named pipes.
  if (!S_ISFIFO(statbuf.st_mode)) {
    LOG(ERROR) << "expected anonymous pipe";
    return -EINVAL;
  }

  inode = statbuf.st_ino;
  return 0;
}

VirtGpuChannel::~VirtGpuChannel() {
  if (query_ring_addr_ != MAP_FAILED)
    munmap(query_ring_addr_, PAGE_SIZE);

  if (channel_ring_addr_ != MAP_FAILED)
    munmap(channel_ring_addr_, PAGE_SIZE);

  // An unwritten rule for the DRM subsystem is a valid GEM valid must be
  // non-zero.  Checkout drm_gem_handle_create_tail in the kernel.
  if (query_ring_handle_)
    close_gem_handle(query_ring_handle_);

  if (channel_ring_handle_)
    close_gem_handle(channel_ring_handle_);

  if (virtgpu_ >= 0)
    close(virtgpu_);
}

int32_t VirtGpuChannel::init() {
  int32_t ret;
  char* drm_device = nullptr;
  uint32_t supports_wayland;
  struct drm_virtgpu_get_caps args = {};
  struct CrossDomainCapabilities cross_domain_caps = {};

  virtgpu_ = open_virtgpu(&drm_device);
  if (virtgpu_ < 0) {
    LOG(ERROR) << "failed to open virtgpu: " << strerror(errno);
    return -errno;
  }

  // Not needed by the VirtGpuChannel.
  free(drm_device);

  struct virtgpu_param params[REQUIRED_PARAMS_SIZE] = {
      PARAM(VIRTGPU_PARAM_3D_FEATURES),
      PARAM(VIRTGPU_PARAM_CAPSET_QUERY_FIX),
      PARAM(VIRTGPU_PARAM_RESOURCE_BLOB),
      PARAM(VIRTGPU_PARAM_HOST_VISIBLE),
      PARAM(VIRTGPU_PARAM_CONTEXT_INIT),
      PARAM(VIRTGPU_PARAM_SUPPORTED_CAPSET_IDs),
  };

  for (const auto& param : params) {
    struct drm_virtgpu_getparam get_param = {};

    get_param.param = param.param;
    get_param.value = (uint64_t)(uintptr_t)&param.value;
    ret = drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_GETPARAM, &get_param);
    if (ret < 0) {
      LOG(ERROR) << "DRM_IOCTL_VIRTGPU_GET_PARAM failed with "
                 << strerror(errno);
      close(virtgpu_);
      virtgpu_ = -1;
      return -EINVAL;
    }

    if (param.param == VIRTGPU_PARAM_SUPPORTED_CAPSET_IDs) {
      if ((param.value & (1 << CAPSET_CROSS_DOMAIN)) == 0)
        return -ENOTSUP;
    }
  }

  args.cap_set_id = CAPSET_CROSS_DOMAIN;
  args.size = sizeof(struct CrossDomainCapabilities);
  args.addr = (uint64_t)&cross_domain_caps;

  ret = drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_GET_CAPS, &args);
  if (ret) {
    LOG(ERROR) << "DRM_IOCTL_VIRTGPU_GET_CAPS failed with " << strerror(errno);
    return ret;
  }

  if (cross_domain_caps.supports_dmabuf)
    supports_dmabuf_ = true;

  supports_wayland = cross_domain_caps.supported_channels &
                     (1 << CROSS_DOMAIN_CHANNEL_TYPE_WAYLAND);

  if (!supports_wayland) {
    LOG(ERROR) << "Wayland support not present on host";
    return -ENOTSUP;
  }

  return 0;
}

bool VirtGpuChannel::supports_dmabuf() {
  return supports_dmabuf_;
}

int32_t VirtGpuChannel::create_ring(uint32_t& out_handle,
                                    uint32_t& out_res_id,
                                    void*& out_addr) {
  int32_t ret = 0;
  struct drm_virtgpu_resource_create_blob drm_rc_blob = {};
  struct drm_virtgpu_map map = {};

  drm_rc_blob.size = PAGE_SIZE;
  drm_rc_blob.blob_mem = VIRTGPU_BLOB_MEM_GUEST;
  drm_rc_blob.blob_flags = VIRTGPU_BLOB_FLAG_USE_MAPPABLE;

  ret =
      drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE_BLOB, &drm_rc_blob);
  if (ret < 0) {
    LOG(ERROR) << "DRM_VIRTGPU_RESOURCE_CREATE_BLOB failed with "
               << strerror(errno);
    return ret;
  }

  out_handle = drm_rc_blob.bo_handle;
  out_res_id = drm_rc_blob.res_handle;

  // Map shared ring buffer.
  map.handle = out_handle;
  ret = drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_MAP, &map);
  if (ret < 0) {
    LOG(ERROR) << "DRM_IOCTL_VIRTGPU_MAP failed with " << strerror(errno);
    return ret;
  }

  out_addr = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                  virtgpu_, map.offset);

  if (out_addr == MAP_FAILED) {
    LOG(ERROR) << "mmap failed with " << strerror(errno);
    return ret;
  }

  return 0;
}

int32_t VirtGpuChannel::create_context(int& out_channel_fd) {
  int ret;
  struct drm_virtgpu_context_init init = {};
  struct drm_virtgpu_context_set_param ctx_set_params[3] = {};
  struct CrossDomainInit cmd_init = {};

  uint32_t query_ring_res_id = 0;
  uint32_t channel_ring_res_id = 0;

  // Initialize the cross domain context.  Create one fence context to wait for
  // metadata queries.
  ctx_set_params[0].param = VIRTGPU_CONTEXT_PARAM_CAPSET_ID;
  ctx_set_params[0].value = CAPSET_CROSS_DOMAIN;
  ctx_set_params[1].param = VIRTGPU_CONTEXT_PARAM_NUM_RINGS;
  ctx_set_params[1].value = 2;
  ctx_set_params[2].param = VIRTGPU_CONTEXT_PARAM_POLL_RINGS_MASK;
  ctx_set_params[2].value = 1 << CROSS_DOMAIN_CHANNEL_RING;

  init.ctx_set_params = (uint64_t)&ctx_set_params[0];
  init.num_params = 3;
  ret = drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_CONTEXT_INIT, &init);
  if (ret) {
    LOG(ERROR) << "DRM_IOCTL_VIRTGPU_CONTEXT_INIT failed with "
               << strerror(errno);
    return ret;
  }

  // Create a shared ring buffer to read metadata queries.
  ret = create_ring(query_ring_handle_, query_ring_res_id, query_ring_addr_);
  if (ret)
    return ret;

  // Create a shared ring buffer to read channel responses
  ret = create_ring(channel_ring_handle_, channel_ring_res_id,
                    channel_ring_addr_);
  if (ret)
    return ret;

  // Notify host about ring buffer
  cmd_init.hdr.cmd = CROSS_DOMAIN_CMD_INIT;
  cmd_init.hdr.cmd_size = sizeof(struct CrossDomainInit);
  cmd_init.query_ring_id = query_ring_res_id;
  cmd_init.channel_ring_id = channel_ring_res_id;
  cmd_init.channel_type = CROSS_DOMAIN_CHANNEL_TYPE_WAYLAND;
  ret = submit_cmd(reinterpret_cast<uint32_t*>(&cmd_init),
                   cmd_init.hdr.cmd_size, CROSS_DOMAIN_RING_NONE, 0, false);
  if (ret < 0)
    return ret;

  // Start polling right after initialization
  ret = channel_poll();
  if (ret < 0)
    return ret;

  out_channel_fd = virtgpu_;
  return 0;
}

int32_t VirtGpuChannel::create_pipe(int& out_pipe_fd) {
  // The guest generates IDs for Wayland read pipes.
  read_pipe_id_++;
  // Account for overflow.
  read_pipe_id_ = std::max(read_pipe_id_, CROSS_DOMAIN_PIPE_READ_START);

  return create_pipe_internal(out_pipe_fd, read_pipe_id_,
                              CROSS_DOMAIN_ID_TYPE_READ_PIPE);
}

int32_t VirtGpuChannel::send(const struct WaylandSendReceive& send) {
  int32_t ret;
  uint8_t cmd_buffer[DEFAULT_BUFFER_SIZE];

  struct CrossDomainSendReceive* cmd_send =
      (struct CrossDomainSendReceive*)cmd_buffer;

  void* send_data = &cmd_buffer[sizeof(struct CrossDomainSendReceive)];

  memset(cmd_send, 0, sizeof(struct CrossDomainSendReceive));

  if (send.data_size > max_send_size())
    return -EINVAL;

  if (send.num_fds > CROSS_DOMAIN_MAX_IDENTIFIERS)
    return -EINVAL;

  cmd_send->hdr.cmd = CROSS_DOMAIN_CMD_SEND;
  cmd_send->hdr.cmd_size =
      sizeof(struct CrossDomainSendReceive) + send.data_size;

  memcpy(send_data, send.data, send.data_size);
  cmd_send->opaque_data_size = send.data_size;

  for (uint32_t i = 0; i < CROSS_DOMAIN_MAX_IDENTIFIERS; i++) {
    if (i >= send.num_fds)
      break;

    ret = fd_analysis(send.fds[i], cmd_send->identifiers[i],
                      cmd_send->identifier_types[i]);
    if (ret)
      return ret;

    cmd_send->num_identifiers++;
  }

  ret = submit_cmd(reinterpret_cast<uint32_t*>(cmd_send),
                   cmd_send->hdr.cmd_size, CROSS_DOMAIN_RING_NONE, 0, false);
  if (ret < 0)
    return ret;

  return 0;
}

int32_t VirtGpuChannel::handle_channel_event(
    enum WaylandChannelEvent& event_type,
    struct WaylandSendReceive& receive,
    int& out_read_pipe) {
  int32_t ret;
  struct CrossDomainHeader* cmd_hdr =
      (struct CrossDomainHeader*)channel_ring_addr_;
  ssize_t bytes_read;
  struct drm_event dummy_event;

  bytes_read = read(virtgpu_, &dummy_event, sizeof(struct drm_event));
  if (bytes_read < static_cast<int>(sizeof(struct drm_event))) {
    LOG(ERROR) << "invalid event size";
    return -EINVAL;
  }

  if (dummy_event.type != VIRTGPU_EVENT_FENCE_SIGNALED) {
    LOG(ERROR) << "invalid event type";
    return -EINVAL;
  }

  if (cmd_hdr->cmd == CROSS_DOMAIN_CMD_RECEIVE) {
    event_type = WaylandChannelEvent::Receive;
    ret = handle_receive(event_type, receive, out_read_pipe);
    if (ret)
      return ret;
  } else if (cmd_hdr->cmd == CROSS_DOMAIN_CMD_READ) {
    event_type = WaylandChannelEvent::Read;
    ret = handle_read();
    if (ret)
      return ret;
  } else {
    return -EINVAL;
  }

  // Start polling again
  ret = channel_poll();
  if (ret < 0)
    return ret;

  return 0;
}

int32_t VirtGpuChannel::allocate(
    const struct WaylandBufferCreateInfo& create_info,
    struct WaylandBufferCreateOutput& create_output) {
  int32_t ret;
  uint64_t blob_id;

  ret = image_query(create_info, create_output, blob_id);
  if (ret < 0) {
    LOG(ERROR) << "image query failed";
    return ret;
  }

  return create_host_blob(blob_id, create_output.host_size, create_output.fd);
}

int32_t VirtGpuChannel::sync(int dmabuf_fd, uint64_t flags) {
  // Unimplemented for now, but just need CROSS_DOMAIN_CMD_SYNC.
  return 0;
}

int32_t VirtGpuChannel::handle_pipe(int read_fd, bool readable, bool& hang_up) {
  uint8_t cmd_buffer[DEFAULT_BUFFER_SIZE];
  ssize_t bytes_read;
  int ret;
  size_t index;

  struct CrossDomainReadWrite* cmd_write =
      (struct CrossDomainReadWrite*)cmd_buffer;

  void* write_data = &cmd_buffer[sizeof(struct CrossDomainReadWrite)];

  memset(cmd_write, 0, sizeof(struct CrossDomainReadWrite));

  cmd_write->hdr.cmd = CROSS_DOMAIN_CMD_WRITE;
  cmd_write->identifier = 0xffffffff;

  ret = pipe_lookup(CROSS_DOMAIN_ID_TYPE_WRITE_PIPE, cmd_write->identifier,
                    read_fd, index);
  if (ret < 0)
    return -EINVAL;

  if (readable) {
    bytes_read = read(read_fd, write_data, MAX_WRITE_SIZE);
    if (bytes_read > 0) {
      cmd_write->opaque_data_size = bytes_read;

      if ((size_t)bytes_read < MAX_WRITE_SIZE)
        hang_up = true;
      else
        hang_up = false;

    } else if (bytes_read == 0) {
      hang_up = true;
    } else {
      return -EINVAL;
    }
  }

  cmd_write->hdr.cmd_size =
      sizeof(struct CrossDomainReadWrite) + cmd_write->opaque_data_size;
  cmd_write->hang_up = hang_up;

  ret = submit_cmd(reinterpret_cast<uint32_t*>(cmd_write),
                   cmd_write->hdr.cmd_size, CROSS_DOMAIN_RING_NONE, 0, false);
  if (ret < 0)
    return ret;

  if (hang_up) {
    close(read_fd);
    std::swap(pipe_cache_[index], pipe_cache_.back());
    pipe_cache_.pop_back();
  }

  return 0;
}

int32_t VirtGpuChannel::submit_cmd(uint32_t* cmd,
                                   uint32_t size,
                                   uint32_t ring_idx,
                                   uint32_t ring_handle,
                                   bool wait) {
  int32_t ret;
  struct drm_virtgpu_3d_wait wait_3d = {};
  struct drm_virtgpu_execbuffer exec = {};

  exec.command = (uint64_t)&cmd[0];
  exec.size = size;
  if (ring_idx != CROSS_DOMAIN_RING_NONE) {
    exec.flags = VIRTGPU_EXECBUF_RING_IDX;
    exec.ring_idx = ring_idx;
  }

  if (ring_handle != 0) {
    exec.bo_handles = (uint64_t)&ring_handle;
    exec.num_bo_handles = 1;
  }

  ret = drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_EXECBUFFER, &exec);
  if (ret < 0) {
    LOG(ERROR) << "DRM_IOCTL_VIRTGPU_EXECBUFFER failed with "
               << strerror(errno);
    return -EINVAL;
  }

  // This is the most traditional way to wait for virtgpu to be finished.  We
  // submit a list of handles to the GPU, and wait for the GPU to be done
  // processing them.  In our case, the handle is the shared ring buffer between
  // the guest proxy (Sommelier) and host compositor proxy (cross domain context
  // type in crosvm).  More sophistication will be needed in the future if the
  // virtgpu approach has any hope of success.
  if (wait) {
    ret = -EAGAIN;
    while (ret == -EAGAIN) {
      wait_3d.handle = ring_handle;
      ret = drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_WAIT, &wait_3d);
    }
  }

  if (ret < 0) {
    LOG(ERROR) << "DRM_IOCTL_VIRTGPU_WAIT failed with " << strerror(errno);
    return ret;
  }

  return 0;
}

int32_t VirtGpuChannel::image_query(const struct WaylandBufferCreateInfo& input,
                                    struct WaylandBufferCreateOutput& output,
                                    uint64_t& blob_id) {
  int32_t ret = 0;
  uint32_t* addr = reinterpret_cast<uint32_t*>(query_ring_addr_);
  struct CrossDomainGetImageRequirements cmd_get_reqs = {};
  struct BufferDescription new_desc = {};

  // Sommelier is single threaded, so no need for locking.
  for (const auto& desc : description_cache_) {
    if (desc.input.width == input.width && desc.input.height == input.height &&
        desc.input.drm_format == input.drm_format) {
      memcpy(&output, &desc.output, sizeof(struct WaylandBufferCreateOutput));
      blob_id = (uint64_t)desc.blob_id;
      return 0;
    }
  }

  cmd_get_reqs.hdr.cmd = CROSS_DOMAIN_CMD_GET_IMAGE_REQUIREMENTS;
  cmd_get_reqs.hdr.cmd_size = sizeof(struct CrossDomainGetImageRequirements);

  cmd_get_reqs.width = input.width;
  cmd_get_reqs.height = input.height;
  cmd_get_reqs.drm_format = input.drm_format;

  // Assumes a gbm-like API on the host
  cmd_get_reqs.flags = GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT;

  ret = submit_cmd(reinterpret_cast<uint32_t*>(&cmd_get_reqs),
                   cmd_get_reqs.hdr.cmd_size, CROSS_DOMAIN_QUERY_RING,
                   query_ring_handle_, true);
  if (ret < 0)
    return ret;

  new_desc.output.fd = -1;
  memcpy(&new_desc.input, &input, sizeof(struct WaylandBufferCreateInfo));
  memcpy(&new_desc.output.strides, &addr[0], 4 * sizeof(uint32_t));
  memcpy(&new_desc.output.offsets, &addr[4], 4 * sizeof(uint32_t));
  memcpy(&new_desc.output.host_size, &addr[10], sizeof(uint64_t));
  memcpy(&new_desc.blob_id, &addr[12], sizeof(uint32_t));

  // Sanity check
  if (!input.dmabuf) {
    if (new_desc.output.host_size < input.size) {
      LOG(ERROR) << "invalid host size";
      return -EINVAL;
    }
  }

  memcpy(&output.strides, &new_desc.output.strides, 4 * sizeof(uint32_t));
  memcpy(&output.offsets, &new_desc.output.offsets, 4 * sizeof(uint32_t));
  output.host_size = new_desc.output.host_size;
  blob_id = (uint64_t)new_desc.blob_id;

  description_cache_.push_back(new_desc);
  return 0;
}

int32_t VirtGpuChannel::close_gem_handle(uint32_t gem_handle) {
  int32_t ret;
  struct drm_gem_close gem_close = {};

  gem_close.handle = gem_handle;
  ret = drmIoctl(virtgpu_, DRM_IOCTL_GEM_CLOSE, &gem_close);
  if (ret) {
    LOG(ERROR) << "DRM_IOCTL_GEM_CLOSE failed (handle=" << std::hex
               << gem_handle << ") error " << strerror(errno);
    return -errno;
  }

  return 0;
}

int32_t VirtGpuChannel::channel_poll() {
  int32_t ret;
  struct CrossDomainPoll cmd_poll = {};

  cmd_poll.hdr.cmd = CROSS_DOMAIN_CMD_POLL;
  cmd_poll.hdr.cmd_size = sizeof(struct CrossDomainPoll);

  // Specifying `channel_ring_handle_` is unnecessary.  The waiting mechanism
  // is `VIRTGPU_CONTEXT_PARAM_POLL_RINGS_MASK` for channel responses.
  ret = submit_cmd(reinterpret_cast<uint32_t*>(&cmd_poll),
                   cmd_poll.hdr.cmd_size, CROSS_DOMAIN_CHANNEL_RING, 0, false);
  if (ret < 0)
    return ret;

  return 0;
}

int32_t VirtGpuChannel::create_host_blob(uint64_t blob_id,
                                         uint64_t size,
                                         int& out_fd) {
  int32_t ret;
  struct drm_virtgpu_resource_create_blob drm_rc_blob = {};

  drm_rc_blob.size = size;
  drm_rc_blob.blob_mem = VIRTGPU_BLOB_MEM_HOST3D;
  drm_rc_blob.blob_flags =
      VIRTGPU_BLOB_FLAG_USE_MAPPABLE | VIRTGPU_BLOB_FLAG_USE_SHAREABLE;
  drm_rc_blob.blob_id = blob_id;

  ret =
      drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE_BLOB, &drm_rc_blob);
  if (ret < 0) {
    LOG(ERROR) << "DRM_VIRTGPU_RESOURCE_CREATE_BLOB failed with "
               << strerror(errno);
    return -errno;
  }

  ret = drmPrimeHandleToFD(virtgpu_, drm_rc_blob.bo_handle,
                           DRM_CLOEXEC | DRM_RDWR, &out_fd);
  if (ret < 0) {
    LOG(ERROR) << "drmPrimeHandleToFD failed with with " << strerror(errno);
    return -errno;
  }

  // dma-buf owns the reference to underlying memory now.
  ret = close_gem_handle(drm_rc_blob.bo_handle);
  if (ret < 0)
    return ret;

  return 0;
}

int32_t VirtGpuChannel::create_fd(uint32_t identifier,
                                  uint32_t identifier_type,
                                  uint32_t identifier_size,
                                  int& out_fd) {
  if (identifier_type == CROSS_DOMAIN_ID_TYPE_VIRTGPU_BLOB) {
    return create_host_blob((uint64_t)identifier, (uint64_t)identifier_size,
                            out_fd);
  } else if (identifier_type == CROSS_DOMAIN_ID_TYPE_WRITE_PIPE) {
    return create_pipe_internal(out_fd, identifier, identifier_type);
  } else {
    return -EINVAL;
  }
}

int32_t VirtGpuChannel::fd_analysis(int fd,
                                    uint32_t& identifier,
                                    uint32_t& identifier_type) {
  int32_t ret = 0;
  uint32_t gem_handle;
  ret = drmPrimeFDToHandle(virtgpu_, fd, &gem_handle);
  if (ret == 0) {
    struct drm_virtgpu_resource_info drm_res_info = {};
    drm_res_info.bo_handle = gem_handle;

    ret = drmIoctl(virtgpu_, DRM_IOCTL_VIRTGPU_RESOURCE_INFO, &drm_res_info);

    if (ret) {
      LOG(ERROR) << "resource info failed";
      return ret;
    }

    identifier = drm_res_info.res_handle;
    identifier_type = CROSS_DOMAIN_ID_TYPE_VIRTGPU_BLOB;
  } else {
    // If it's not a blob, the only other option is a pipe.  Check to confirm.
    uint32_t inode;
    ret = fstat_pipe(fd, inode);
    if (ret)
      return ret;

    for (const auto& pipe_desc : pipe_cache_) {
      if (pipe_desc.inode == inode) {
        identifier = pipe_desc.identifier;
        identifier_type = pipe_desc.identifier_type;
        return 0;
      }
    }

    return -EINVAL;
  }

  return 0;
}

int32_t VirtGpuChannel::create_pipe_internal(int& out_pipe_fd,
                                             uint32_t identifier,
                                             uint32_t identifier_type) {
  int32_t ret;
  int fds[2];
  struct PipeDescription pipe_desc = {};
  bool return_read_pipe = false;

  // When proxying a Wayland pipe, we return one end to the WaylandChannel
  // consumer and keep one end to ourselves.  Keeping both ends isn't useful.
  if (identifier_type == CROSS_DOMAIN_ID_TYPE_READ_PIPE)
    return_read_pipe = true;
  else if (identifier_type == CROSS_DOMAIN_ID_TYPE_WRITE_PIPE)
    return_read_pipe = false;
  else
    return -EINVAL;

  ret = pipe(fds);
  if (ret < 0) {
    LOG(ERROR) << "pipe creation failed with " << strerror(errno);
    return -errno;
  }

  // The same inode number is used for the read/write ends of the pipe.
  ret = fstat_pipe(fds[0], pipe_desc.inode);
  if (ret < 0)
    return ret;

  pipe_desc.read_fd = fds[0];
  pipe_desc.write_fd = fds[1];
  pipe_desc.identifier = identifier;
  pipe_desc.identifier_type = identifier_type;
  pipe_cache_.push_back(pipe_desc);

  if (return_read_pipe)
    out_pipe_fd = fds[0];
  else
    out_pipe_fd = fds[1];

  return 0;
}

int32_t VirtGpuChannel::handle_receive(enum WaylandChannelEvent& event_type,
                                       struct WaylandSendReceive& receive,
                                       int& out_read_pipe) {
  int ret;
  struct CrossDomainSendReceive* cmd_receive =
      (struct CrossDomainSendReceive*)channel_ring_addr_;

  uint8_t* recv_data = reinterpret_cast<uint8_t*>(channel_ring_addr_) +
                       sizeof(struct CrossDomainSendReceive);

  for (uint32_t i = 0; i < CROSS_DOMAIN_MAX_IDENTIFIERS; i++) {
    if (i < cmd_receive->num_identifiers) {
      ret = create_fd(cmd_receive->identifiers[i],
                      cmd_receive->identifier_types[i],
                      cmd_receive->identifier_sizes[i], receive.fds[i]);
      if (ret)
        return ret;

      receive.num_fds++;

      if (cmd_receive->identifier_types[i] == CROSS_DOMAIN_ID_TYPE_WRITE_PIPE) {
        size_t index;
        int ret = 0;
        if (out_read_pipe >= 0)
          return -EINVAL;

        ret = pipe_lookup(cmd_receive->identifier_types[i],
                          cmd_receive->identifiers[i], out_read_pipe, index);
        if (ret < 0)
          return -EINVAL;

        event_type = WaylandChannelEvent::ReceiveAndProxy;
      }
    } else {
      break;
    }
  }

  if (cmd_receive->opaque_data_size > 0) {
    receive.data =
        reinterpret_cast<uint8_t*>(calloc(1, cmd_receive->opaque_data_size));
    if (!receive.data)
      return -ENOMEM;

    memcpy(receive.data, recv_data, cmd_receive->opaque_data_size);
  }

  receive.data_size = cmd_receive->opaque_data_size;
  return 0;
}

int32_t VirtGpuChannel::handle_read() {
  int write_fd = -1;
  int ret = 0;
  ssize_t bytes_written;
  size_t index;
  struct CrossDomainReadWrite* cmd_read =
      (struct CrossDomainReadWrite*)channel_ring_addr_;

  uint8_t* read_data = reinterpret_cast<uint8_t*>(channel_ring_addr_) +
                       sizeof(struct CrossDomainReadWrite);

  ret = pipe_lookup(CROSS_DOMAIN_ID_TYPE_READ_PIPE, cmd_read->identifier,
                    write_fd, index);
  if (ret < 0)
    return -EINVAL;

  bytes_written = write(write_fd, read_data, cmd_read->opaque_data_size);

  if (bytes_written < (ssize_t)cmd_read->opaque_data_size) {
    LOG(ERROR) << "failed to write all necessary bytes";
    return -EINVAL;
  }

  if (cmd_read->hang_up) {
    close(write_fd);
    std::swap(pipe_cache_[index], pipe_cache_.back());
    pipe_cache_.pop_back();
  }

  return 0;
}

int32_t VirtGpuChannel::pipe_lookup(uint32_t identifier_type,
                                    uint32_t& identifier,
                                    int& fd,
                                    size_t& index) {
  index = 0;
  for (const auto& pipe_desc : pipe_cache_) {
    if (pipe_desc.identifier == identifier) {
      // The host and guest are proxying the read operation, need to write to
      // internally owned file descriptor.
      if (identifier_type == CROSS_DOMAIN_ID_TYPE_READ_PIPE) {
        fd = pipe_desc.write_fd;
        return 0;
      }

      // The host and guest are proxying the write operation, need to read from
      // internally owned file descriptor.
      if (identifier_type == CROSS_DOMAIN_ID_TYPE_WRITE_PIPE) {
        fd = pipe_desc.read_fd;
        return 0;
      }
    }

    if (fd == pipe_desc.read_fd || fd == pipe_desc.write_fd) {
      identifier = pipe_desc.identifier;
      return 0;
    }

    index++;
  }

  return -EINVAL;
}

size_t VirtGpuChannel::max_send_size() {
  return MAX_SEND_SIZE;
}
