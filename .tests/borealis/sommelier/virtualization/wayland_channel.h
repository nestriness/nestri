// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef VM_TOOLS_SOMMELIER_VIRTUALIZATION_WAYLAND_CHANNEL_H_
#define VM_TOOLS_SOMMELIER_VIRTUALIZATION_WAYLAND_CHANNEL_H_

#include <cstdint>
#include <sys/mman.h>
#include <vector>

#include "virtgpu_cross_domain_protocol.h"

/*
 * Copied from `VIRTWL_SEND_MAX_ALLOCS`.  It was originally set this way
 * because it seemed like a reasonable limit.
 */
#define WAYLAND_MAX_FDs 28

// Default buffer size based on the size of a typical page.
#define DEFAULT_BUFFER_SIZE 4096

struct WaylandSendReceive {
  int channel_fd;

  int fds[WAYLAND_MAX_FDs];
  uint32_t num_fds;
  uint8_t* data;
  size_t data_size;
};

enum WaylandChannelEvent {
  None,
  Receive,
  ReceiveAndProxy,
  Read,
};

struct WaylandBufferCreateInfo {
  /*
   * If true, create a dmabuf on the host.  If not, create a shared memory
   * region.  A dmabuf can be scanned out by the display engine directly,
   * enabling zero copy.  A shared memory region necessitates a copy to a
   * dma-buf by the host compositor.
   */
  bool dmabuf;

  /*
   * dma-buf parameters.  The allocation is done by host minigbm and used when
   * crosvm is built with the "wl-dmabuf" feature and virtgpu 3d is not
   * enabled.  The modifier is not present, because we only want to allocate
   * linear zero-copy buffers in this case.  The modifier makes sense when
   * virtgpu 3d is enabled, but in that case guest Mesa gbm (backed by Virgl)
   * allocates the resource, not sommelier.
   */
  uint32_t width;
  uint32_t height;
  uint32_t drm_format;

  /*
   * Shared memory region parameters.  The allocation is done by memfd(..) on
   * the host.
   */
  uint32_t size;
};

/*
 * Linux mode-setting APIs [drmModeAddFB2(..)] and Wayland normally specify
 * four planes, even though three are used in practice.  Follow that convention
 * here.
 */
struct WaylandBufferCreateOutput {
  int fd;
  uint32_t offsets[4];
  uint32_t strides[4];
  uint64_t host_size;
};

class WaylandChannel {
 public:
  WaylandChannel() = default;
  virtual ~WaylandChannel() = default;

  // Initializes the Wayland Channel.  Returns 0 on success, -errno on failure.
  virtual int32_t init() = 0;

  // Returns true if the Wayland channel supports dmabuf, false otherwise.  If
  // dmabuf is supported, Sommelier will use the `zwp_linux_dmabuf_v1`
  // protocol.
  virtual bool supports_dmabuf() = 0;

  // Creates a new context for handling the wayland command stream.  Returns 0
  // on success, and a pollable `out_channel_fd`.  This fd represents the
  // connection to the host compositor, and used for subsequent `send` and
  // `receive` operations.
  //
  // Returns -errno on failure.
  virtual int32_t create_context(int& out_channel_fd) = 0;

  // Creates a new clipboard pipe for Wayland input.  Note this interface can't
  // wrap a call to "pipe", and is named based on VIRTWL_IOCTL_NEW_PIPE.  A new
  // interface may be designed in the future.
  //
  // Returns 0 on success, and a readable `out_pipe_fd`.
  // Returns -errno on failure.
  virtual int32_t create_pipe(int& out_pipe_fd) = 0;

  // Sends fds and associated commands to the host [like sendmsg(..)].  The fds
  // are converted to host handles using an implementation specific method.
  // For virtwl, either:
  //  (a) virtwl allocated resources are sent.
  //  (b) The virtgpu resource handle is fished out by virtwl.
  //
  // Returns 0 on success.  Returns -errno on failure.  If `send.data_size` is
  // than greater zero, then the caller must provide a pointer to valid memory
  // in `send.data`.
  virtual int32_t send(const struct WaylandSendReceive& send) = 0;

  // Handles a poll event on the channel file descriptor.
  //
  // Returns 0 on success.  Returns -errno on failure.  On success, the type of
  // event is given by `event_type`.
  //
  // If `event_type` is WaylandChannelEvent::Receive, the caller must forward
  // received fds and associated commands to the client.
  //
  // If `event_type` is WaylandChannelEvent::ReceiveAndProxy, `out_read_pipe`
  // is also returned in addition to the `receive` data.  The caller does not
  // take ownership of `out_read_pipe`.  The caller must poll `out_read_pipe`
  // in addition to forwarding the data given by `receive`.  The `handle_pipe`
  // function must be called the case of `out_read_pipe` event.
  //
  // In both above cases, if the returned `receive.data_size` is than greater
  // zero, then the caller takes ownership of `receive.data` and must free(..)
  // the memory when appropriate.
  //
  // If `event_type` is WaylandChannelEvent::Read, then both `out_read_pipe` and
  // `receive` are meaningless. The implementation handles the event internally.
  virtual int32_t handle_channel_event(enum WaylandChannelEvent& event_type,
                                       struct WaylandSendReceive& receive,
                                       int& out_read_pipe) = 0;

  // Allocates a shared memory resource or dma-buf on the host.  Maps it into
  // the guest.  The intended use case for this function is sharing resources
  // with the host compositor when virtgpu 3d is not enabled.
  //
  // Returns 0 on success.  Returns -errno on success.
  virtual int32_t allocate(const struct WaylandBufferCreateInfo& create_info,
                           struct WaylandBufferCreateOutput& create_output) = 0;

  // Synchronizes accesses to previously created host dma-buf.
  // Returns 0 on success.  Returns -errno on failure.
  virtual int32_t sync(int dmabuf_fd, uint64_t flags) = 0;

  // Reads from the specified `read_fd` and forwards to the host if `readable`
  // is true.  Closes the `read_fd` and the proxied write fd on the host if
  // `hang_up` is true and all the data has been read.
  //
  // `read_fd` *must* be a read pipe given by `handle_channel_event` when the
  // `event_type` is WaylandChannelEvent::ReceiveAndProxy.
  virtual int32_t handle_pipe(int read_fd, bool readable, bool& hang_up) = 0;

  // Returns the maximum size of opaque data that the channel is able to handle
  // in the `send` function.  Must be less than or equal to DEFAULT_BUFFER_SIZE.
  virtual size_t max_send_size() = 0;
};

class VirtWaylandChannel : public WaylandChannel {
 public:
  VirtWaylandChannel() : virtwl_{-1}, supports_dmabuf_(false) {}
  ~VirtWaylandChannel() override;

  int32_t init() override;
  bool supports_dmabuf() override;
  int32_t create_context(int& out_channel_fd) override;
  int32_t create_pipe(int& out_pipe_fd) override;
  int32_t send(const struct WaylandSendReceive& send) override;
  int32_t handle_channel_event(enum WaylandChannelEvent& event_type,
                               struct WaylandSendReceive& receive,
                               int& out_read_pipe) override;

  int32_t allocate(const struct WaylandBufferCreateInfo& create_info,
                   struct WaylandBufferCreateOutput& create_output) override;

  int32_t sync(int dmabuf_fd, uint64_t flags) override;
  int32_t handle_pipe(int read_fd, bool readable, bool& hang_up) override;
  size_t max_send_size() override;

 private:
  // virtwl device file descriptor
  int32_t virtwl_;
  bool supports_dmabuf_;
};

class VirtGpuChannel : public WaylandChannel {
 public:
  VirtGpuChannel()
      : virtgpu_{-1},
        query_ring_addr_{MAP_FAILED},
        query_ring_handle_{},
        channel_ring_addr_{MAP_FAILED},
        channel_ring_handle_{},
        supports_dmabuf_(false),
        read_pipe_id_{CROSS_DOMAIN_PIPE_READ_START} {}
  ~VirtGpuChannel() override;

  int32_t init() override;
  bool supports_dmabuf() override;
  int32_t create_context(int& out_channel_fd) override;
  int32_t create_pipe(int& out_pipe_fd) override;
  int32_t send(const struct WaylandSendReceive& send) override;
  int32_t handle_channel_event(enum WaylandChannelEvent& event_type,
                               struct WaylandSendReceive& receive,
                               int& out_read_pipe) override;

  int32_t allocate(const struct WaylandBufferCreateInfo& create_info,
                   struct WaylandBufferCreateOutput& create_output) override;

  int32_t sync(int dmabuf_fd, uint64_t flags) override;
  int32_t handle_pipe(int read_fd, bool readable, bool& hang_up) override;
  size_t max_send_size() override;

 private:
  /*
   * This provides the full description of the buffer -- width, height, strides,
   * offsets and host_size.  Meant for internal virtgpu channel use only.
   */
  struct BufferDescription {
    struct WaylandBufferCreateInfo input;
    struct WaylandBufferCreateOutput output;
    uint32_t blob_id;
  };

  /*
   * Provides the read end and write end of a pipe, along with the inode (a
   * guest unique identifier) and host descriptor id;
   */
  struct PipeDescription {
    int read_fd;
    int write_fd;
    uint32_t identifier_type;
    uint32_t inode;
    uint32_t identifier;
  };

  int32_t image_query(const struct WaylandBufferCreateInfo& input,
                      struct WaylandBufferCreateOutput& output,
                      uint64_t& blob_id);

  int32_t submit_cmd(uint32_t* cmd,
                     uint32_t size,
                     uint32_t ring_idx,
                     uint32_t ring_handle,
                     bool wait);
  int32_t channel_poll();
  int32_t close_gem_handle(uint32_t gem_handle);
  int32_t create_host_blob(uint64_t blob_id, uint64_t size, int& out_fd);

  int32_t fd_analysis(int fd, uint32_t& identifier, uint32_t& identifier_type);

  int32_t create_fd(uint32_t identifier,
                    uint32_t identifier_type,
                    uint32_t identifier_size,
                    int& out_fd);

  int32_t create_pipe_internal(int& out_pipe_fd,
                               uint32_t identifier,
                               uint32_t identifier_type);

  int32_t handle_receive(enum WaylandChannelEvent& event_type,
                         struct WaylandSendReceive& receive,
                         int& out_read_pipe);

  int32_t handle_read();

  int32_t pipe_lookup(uint32_t identifier_type,
                      uint32_t& identifier,
                      int& fd,
                      size_t& index);

  int32_t create_ring(uint32_t& out_handle,
                      uint32_t& out_res_id,
                      void*& out_addr);

  int32_t virtgpu_;

  void* query_ring_addr_;
  uint32_t query_ring_handle_;

  void* channel_ring_addr_;
  uint32_t channel_ring_handle_;

  bool supports_dmabuf_;
  // Largest client-allocated ID so far, starts at 0x80000000
  // to avoid conflicts with the host.
  uint32_t read_pipe_id_;

  std::vector<BufferDescription> description_cache_;
  std::vector<PipeDescription> pipe_cache_;
};

int open_virtgpu(char** drm_device);

#endif  // VM_TOOLS_SOMMELIER_VIRTUALIZATION_WAYLAND_CHANNEL_H_
