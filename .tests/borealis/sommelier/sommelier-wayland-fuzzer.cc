// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <wayland-client.h>

#include "sommelier.h"                       // NOLINT(build/include_directory)
#include "sommelier-ctx.h"                   // NOLINT(build/include_directory)
#include "sommelier-logging.h"               // NOLINT(build/include_directory)
#include "virtualization/wayland_channel.h"  // NOLINT(build/include_directory)

class FuzzChannel : public WaylandChannel {
 private:
  int recv_fd = -1;

 public:
  int send_fd = -1;

  ~FuzzChannel() {
    if (send_fd != -1) {
      close(send_fd);
    }
    if (recv_fd != -1) {
      close(recv_fd);
    }
  }

  int32_t init() override { return 0; }

  bool supports_dmabuf() override { return false; }

  int32_t create_context(int& out_channel_fd) override {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv)) {
      return -errno;
    }

    send_fd = sv[0];
    recv_fd = sv[1];
    out_channel_fd = sv[1];
    return 0;
  }

  int32_t create_pipe(int& out_pipe_fd) override { return -1; }

  int32_t send(const struct WaylandSendReceive& send) override { return 0; }

  int32_t handle_channel_event(enum WaylandChannelEvent& event_type,
                               struct WaylandSendReceive& receive,
                               int& out_read_pipe) override {
    uint8_t* buffer = static_cast<uint8_t*>(malloc(DEFAULT_BUFFER_SIZE));
    int bytes = recv(receive.channel_fd, buffer, DEFAULT_BUFFER_SIZE, 0);
    if (bytes < 0) {
      free(buffer);
      return -errno;
    }

    receive.data = buffer;
    receive.data_size = bytes;
    receive.num_fds = 0;

    event_type = WaylandChannelEvent::Receive;

    return 0;
  }

  int32_t allocate(const struct WaylandBufferCreateInfo& create_info,
                   struct WaylandBufferCreateOutput& create_output) override {
    return -1;
  }

  int32_t sync(int dmabuf_fd, uint64_t flags) override { return 0; }
  int32_t handle_pipe(int read_fd, bool readable, bool& hang_up) override {
    return 0;
  }

  size_t max_send_size(void) override { return DEFAULT_BUFFER_SIZE; }
};

void null_logger(const char*, va_list) {}

class Environment {
 public:
  Environment() {
    wl_log_set_handler_client(null_logger);
    wl_log_set_handler_server(null_logger);
  }
};

static int handle_host_to_sommelier_event(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  int count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
    return 0;
  }

  if (mask & WL_EVENT_READABLE)
    count = wl_display_dispatch(ctx->display);
  if (mask & WL_EVENT_WRITABLE)
    wl_display_flush(ctx->display);

  if (mask == 0) {
    count = wl_display_dispatch_pending(ctx->display);
    wl_display_flush(ctx->display);
  }

  return count;
}

int drain_socket(int fd, uint32_t mask, void* data) {
  uint8_t buffer[DEFAULT_BUFFER_SIZE];
  return recv(fd, buffer, DEFAULT_BUFFER_SIZE, 0) > 0;
}

// Count the number of open file descriptors to make sure we don't leak any.
// Aborts on error, as this should never fail.
int count_fds() {
  DIR* dir = opendir("/proc/self/fd");
  if (!dir) {
    LOG(ERROR) << "failed to open /proc/self/fd: " << strerror(errno);
    abort();
  }

  // Ignore the "." and ".." entries, along with the fd we just opened.
  int count = -3;

  // Needed to distinguish between eof and errors.
  errno = 0;
  while (struct dirent* _ = readdir(dir)) {
    count++;
  }
  if (errno) {
    LOG(ERROR) << "failed to read from /proc/self/fd: " << strerror(errno);
    abort();
  }

  if (closedir(dir)) {
    LOG(ERROR) << "failed to close /proc/self/fd: " << strerror(errno);
    abort();
  }

  return count;
}

int LLVMFuzzerTestOneInput_real(const uint8_t* data, size_t size) {
  int ret;
  static Environment env;
  FuzzedDataProvider source(data, size);

  struct sl_context ctx;
  FuzzChannel channel;
  int host_to_sommelier, client_to_sommelier;

  sl_context_init_default(&ctx);

  // Create a connection from the host to sommelier. This is done via a
  // WaylandChannel implementation that keeps a socketpair internally. One end
  // goes to sommelier to listen on/write to, the other end is kept by us. The
  // channel implements send with a no-op, so we don't ever have to read from
  // our end.
  ret = channel.init();
  assert(!ret);

  struct wl_event_loop* event_loop =
      sl_context_configure_event_loop(&ctx, &channel,
                                      /*use_virtual_context=*/true);

  // `display` takes ownership of `virtwl_display_fd`
  ctx.display = wl_display_connect_to_fd(ctx.virtwl_display_fd);

  // Create a connection from the client to sommelier. One end is passed into
  // libwayland for sommelier to handle, the other end is owned by the
  // fuzzer. We set up the event loop to drain any data send by sommelier to our
  // end, and write fuzz data to our end in the main loop.
  int sv[2];
  ret = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
  assert(!ret);
  // wl_client takes ownership of its file descriptor
  ctx.client = wl_client_create(ctx.host_display, sv[0]);
  std::unique_ptr<struct wl_event_source> drain_event(wl_event_loop_add_fd(
      event_loop, sv[1], WL_EVENT_READABLE, drain_socket, nullptr));

  // Add the host-to-sommelier display to the event loop. The client wayland
  // library doesn't have an event loop system, instead you're expected to
  // integrate into another event loop using wl_display_get_fd if you need to.
  std::unique_ptr<struct wl_event_source> display_event(
      wl_event_loop_add_fd(event_loop, wl_display_get_fd(ctx.display),
                           WL_EVENT_READABLE | WL_EVENT_WRITABLE,
                           handle_host_to_sommelier_event, &ctx));

  // Getting the registry object from the host and listening for events is the
  // top-level task in sommelier. Everything else is driven by messages from the
  // host or the client.
  auto* registry = wl_display_get_registry(ctx.display);
  wl_registry_add_listener(registry, &sl_registry_listener, &ctx);

  host_to_sommelier = channel.send_fd;
  client_to_sommelier = sv[1];

  while (source.remaining_bytes() &&
         !wl_list_empty(wl_display_get_client_list(ctx.host_display)) &&
         !wl_display_get_error(ctx.display)) {
    // Randomly pick a connection to write too.
    int fd = source.ConsumeBool() ? host_to_sommelier : client_to_sommelier;

    // The random length string extractor uses a terminator character to prevent
    // the boundaries of a message from shifting when the fuzzer inserts or
    // removed bytes. This is (probably) better then consuming a length and then
    // consuming that many characters.
    std::string data = source.ConsumeRandomLengthString(DEFAULT_BUFFER_SIZE);

    int sent = send(fd, data.data(), data.length(), 0);
    assert(sent == data.length());

    wl_display_flush_clients(ctx.host_display);
    wl_event_loop_dispatch(event_loop, 0);
  }

  wl_registry_destroy(registry);

  close(ctx.virtwl_socket_fd);
  close(client_to_sommelier);

  ctx.wayland_channel_event_source.reset();
  ctx.virtwl_socket_event_source.reset();
  drain_event.reset();
  display_event.reset();

  wl_display_destroy_clients(ctx.host_display);
  wl_display_destroy(ctx.host_display);

  wl_display_disconnect(ctx.display);

  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  int start_fds = count_fds();

  int ret = LLVMFuzzerTestOneInput_real(data, size);

  int end_fds = count_fds();
  if (start_fds != end_fds) {
    LOG(ERROR) << "leaked " << end_fds - start_fds << " file descriptors!";
    abort();
  }

  return ret;
}
