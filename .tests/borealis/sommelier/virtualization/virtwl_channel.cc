// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../sommelier-logging.h"  // NOLINT(build/include_directory)
#include "linux-headers/virtwl.h"  // NOLINT(build/include_directory)
#include "wayland_channel.h"       // NOLINT(build/include_directory)

#define VIRTWL_DEVICE "/dev/wl0"
#define MAX_SEND_SIZE (DEFAULT_BUFFER_SIZE - sizeof(struct virtwl_ioctl_txn))

VirtWaylandChannel::~VirtWaylandChannel() {
  if (virtwl_ >= 0)
    close(virtwl_);
}

int32_t VirtWaylandChannel::init() {
  virtwl_ = open(VIRTWL_DEVICE, O_RDWR);
  int32_t ret;
  struct WaylandBufferCreateInfo create_info = {};
  struct WaylandBufferCreateOutput create_output = {};
  create_output.fd = -1;

  if (virtwl_ == -1)
    return -errno;

  create_info.dmabuf = true;
  supports_dmabuf_ = true;

  ret = allocate(create_info, create_output);
  if (ret && errno == ENOTTY) {
    LOG(WARNING)
        << "virtwl-dmabuf driver not supported by host, using virtwl instead";
    supports_dmabuf_ = false;
  } else if (create_output.fd >= 0) {
    // Close the returned dmabuf fd in case the invalid dmabuf metadata
    // given above actually manages to return an fd successfully.
    close(create_output.fd);
    create_output.fd = -1;
  }

  return 0;
}

bool VirtWaylandChannel::supports_dmabuf() {
  return supports_dmabuf_;
}

int32_t VirtWaylandChannel::create_context(int& out_channel_fd) {
  int ret;
  struct virtwl_ioctl_new new_ctx = {
      .type = VIRTWL_IOCTL_NEW_CTX,
      .fd = -1,
      .flags = 0,
  };

  ret = ioctl(virtwl_, VIRTWL_IOCTL_NEW, &new_ctx);
  if (ret)
    return -errno;

  out_channel_fd = new_ctx.fd;

  return 0;
}

int32_t VirtWaylandChannel::create_pipe(int& out_pipe_fd) {
  int ret;
  struct virtwl_ioctl_new new_pipe = {
      .type = VIRTWL_IOCTL_NEW_PIPE_READ,
      .fd = -1,
      .flags = 0,
  };

  new_pipe.size = 0;

  ret = ioctl(virtwl_, VIRTWL_IOCTL_NEW, &new_pipe);
  if (ret)
    return -errno;

  out_pipe_fd = new_pipe.fd;

  return 0;
}

int32_t VirtWaylandChannel::send(const struct WaylandSendReceive& send) {
  int ret;
  uint8_t ioctl_buffer[DEFAULT_BUFFER_SIZE];

  struct virtwl_ioctl_txn* txn = (struct virtwl_ioctl_txn*)ioctl_buffer;
  void* send_data = &txn->data;

  if (send.data_size > max_send_size())
    return -EINVAL;

  memcpy(send_data, send.data, send.data_size);

  for (uint32_t i = 0; i < WAYLAND_MAX_FDs; i++) {
    if (i < send.num_fds) {
      txn->fds[i] = send.fds[i];
    } else {
      txn->fds[i] = -1;
    }
  }

  txn->len = send.data_size;
  ret = ioctl(send.channel_fd, VIRTWL_IOCTL_SEND, txn);
  if (ret)
    return -errno;

  return 0;
}

int32_t VirtWaylandChannel::handle_channel_event(
    enum WaylandChannelEvent& event_type,
    struct WaylandSendReceive& receive,
    int& out_read_pipe) {
  int ret;
  uint8_t ioctl_buffer[DEFAULT_BUFFER_SIZE];

  struct virtwl_ioctl_txn* txn = (struct virtwl_ioctl_txn*)ioctl_buffer;
  size_t max_recv_size = sizeof(ioctl_buffer) - sizeof(struct virtwl_ioctl_txn);
  void* recv_data = &txn->data;

  txn->len = max_recv_size;
  ret = ioctl(receive.channel_fd, VIRTWL_IOCTL_RECV, txn);
  if (ret)
    return -errno;

  for (uint32_t i = 0; i < WAYLAND_MAX_FDs; i++) {
    if (txn->fds[i] >= 0) {
      receive.num_fds++;
      receive.fds[i] = txn->fds[i];
    } else {
      break;
    }
  }

  if (txn->len > 0) {
    receive.data = reinterpret_cast<uint8_t*>(calloc(1, txn->len));
    if (!receive.data)
      return -ENOMEM;

    memcpy(receive.data, recv_data, txn->len);
  }

  receive.data_size = txn->len;
  event_type = WaylandChannelEvent::Receive;
  return 0;
}

int32_t VirtWaylandChannel::allocate(
    const struct WaylandBufferCreateInfo& create_info,
    struct WaylandBufferCreateOutput& create_output) {
  int ret;
  struct virtwl_ioctl_new ioctl_new = {};

  if (create_info.dmabuf) {
    ioctl_new.type = VIRTWL_IOCTL_NEW_DMABUF;
    ioctl_new.fd = -1;
    ioctl_new.flags = 0;
    ioctl_new.dmabuf.width = create_info.width;
    ioctl_new.dmabuf.height = create_info.height;
    ioctl_new.dmabuf.format = create_info.drm_format;
  } else {
    ioctl_new.type = VIRTWL_IOCTL_NEW_ALLOC;
    ioctl_new.fd = -1;
    ioctl_new.flags = 0;
    ioctl_new.size = create_info.size;
    create_output.host_size = create_info.size;
  }

  ret = ioctl(virtwl_, VIRTWL_IOCTL_NEW, &ioctl_new);
  if (ret)
    return -errno;

  if (create_info.dmabuf) {
    create_output.strides[0] = ioctl_new.dmabuf.stride0;
    create_output.strides[1] = ioctl_new.dmabuf.stride1;
    create_output.strides[2] = ioctl_new.dmabuf.stride2;

    create_output.offsets[0] = ioctl_new.dmabuf.offset0;
    create_output.offsets[1] = ioctl_new.dmabuf.offset1;
    create_output.offsets[2] = ioctl_new.dmabuf.offset2;

    // The common layer will consider multi-planar sizes as needed.
    create_output.host_size = create_output.strides[0] * create_info.height;
  }

  create_output.fd = ioctl_new.fd;

  return 0;
}

int32_t VirtWaylandChannel::sync(int dmabuf_fd, uint64_t flags) {
  struct virtwl_ioctl_dmabuf_sync sync = {};
  int ret;

  sync.flags = flags;
  ret = ioctl(dmabuf_fd, VIRTWL_IOCTL_DMABUF_SYNC, &sync);
  if (ret)
    return -errno;

  return 0;
}

int32_t VirtWaylandChannel::handle_pipe(int read_fd,
                                        bool readable,
                                        bool& hang_up) {
  return 0;
}

size_t VirtWaylandChannel::max_send_size() {
  return MAX_SEND_SIZE;
}
