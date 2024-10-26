// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-ctx.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-util.h>

#include "aura-shell-client-protocol.h"  // NOLINT(build/include_directory)
#include "sommelier.h"                   // NOLINT(build/include_directory)
#include "sommelier-logging.h"           // NOLINT(build/include_directory)
#include "sommelier-tracing.h"           // NOLINT(build/include_directory)

// TODO(b/173147612): Use container_token rather than this name.
#define DEFAULT_VM_NAME "termina"

// Returns the string mapped to the given ATOM_ enum value.
//
// Note this is NOT the atom value sent via the X protocol, despite both being
// ints. Use |sl_context::atoms| to map between X protocol atoms and ATOM_ enum
// values: If `atoms[i].value = j`, i is the ATOM_ enum value and j is the
// X protocol atom.
//
// If the given value is out of range of the ATOM_ enum, returns nullptr.
const char* sl_context_atom_name(int atom_enum) {
  switch (atom_enum) {
    case ATOM_WM_S0:
      return "WM_S0";
    case ATOM_WM_PROTOCOLS:
      return "WM_PROTOCOLS";
    case ATOM_WM_STATE:
      return "WM_STATE";
    case ATOM_WM_CHANGE_STATE:
      return "WM_CHANGE_STATE";
    case ATOM_WM_DELETE_WINDOW:
      return "WM_DELETE_WINDOW";
    case ATOM_WM_TAKE_FOCUS:
      return "WM_TAKE_FOCUS";
    case ATOM_WM_CLIENT_LEADER:
      return "WM_CLIENT_LEADER";
    case ATOM_WL_SURFACE_ID:
      return "WL_SURFACE_ID";
    case ATOM_UTF8_STRING:
      return "UTF8_STRING";
    case ATOM_MOTIF_WM_HINTS:
      return "_MOTIF_WM_HINTS";
    case ATOM_NET_ACTIVE_WINDOW:
      return "_NET_ACTIVE_WINDOW";
    case ATOM_NET_FRAME_EXTENTS:
      return "_NET_FRAME_EXTENTS";
    case ATOM_NET_STARTUP_ID:
      return "_NET_STARTUP_ID";
    case ATOM_NET_SUPPORTED:
      return "_NET_SUPPORTED";
    case ATOM_NET_SUPPORTING_WM_CHECK:
      return "_NET_SUPPORTING_WM_CHECK";
    case ATOM_NET_WM_NAME:
      return "_NET_WM_NAME";
    case ATOM_NET_WM_MOVERESIZE:
      return "_NET_WM_MOVERESIZE";
    case ATOM_NET_WM_STATE:
      return "_NET_WM_STATE";
    case ATOM_NET_WM_STATE_FULLSCREEN:
      return "_NET_WM_STATE_FULLSCREEN";
    case ATOM_NET_WM_STATE_MAXIMIZED_VERT:
      return "_NET_WM_STATE_MAXIMIZED_VERT";
    case ATOM_NET_WM_STATE_MAXIMIZED_HORZ:
      return "_NET_WM_STATE_MAXIMIZED_HORZ";
    case ATOM_NET_WM_STATE_FOCUSED:
      return "_NET_WM_STATE_FOCUSED";
    case ATOM_NET_WM_WINDOW_TYPE:
      return "_NET_WM_WINDOW_TYPE";
    case ATOM_NET_WM_WINDOW_TYPE_NORMAL:
      return "_NET_WM_WINDOW_TYPE_NORMAL";
    case ATOM_NET_WM_WINDOW_TYPE_DIALOG:
      return "_NET_WM_WINDOW_TYPE_DIALOG";
    case ATOM_NET_WM_WINDOW_TYPE_SPLASH:
      return "_NET_WM_WINDOW_TYPE_SPLASH";
    case ATOM_NET_WM_WINDOW_TYPE_UTILITY:
      return "_NET_WM_WINDOW_TYPE_UTILITY";
    case ATOM_NET_WM_PID:
      return "_NET_WM_PID";
    case ATOM_CLIPBOARD:
      return "CLIPBOARD";
    case ATOM_CLIPBOARD_MANAGER:
      return "CLIPBOARD_MANAGER";
    case ATOM_TARGETS:
      return "TARGETS";
    case ATOM_TIMESTAMP:
      return "TIMESTAMP";
    case ATOM_TEXT:
      return "TEXT";
    case ATOM_INCR:
      return "INCR";
    case ATOM_WL_SELECTION:
      return "_WL_SELECTION";
    case ATOM_GTK_THEME_VARIANT:
      return "_GTK_THEME_VARIANT";
    case ATOM_STEAM_GAME:
      return "STEAM_GAME";
    case ATOM_XWAYLAND_RANDR_EMU_MONITOR_RECTS:
      return "_XWAYLAND_RANDR_EMU_MONITOR_RECTS";
    case ATOM_SOMMELIER_QUIRK_APPLIED:
      return "SOMMELIER_QUIRK_APPLIED";
  }
  return nullptr;
}

void sl_context_init_default(struct sl_context* ctx) {
  *ctx = {0};
  ctx->runprog = nullptr;
  ctx->display = nullptr;
  ctx->host_display = nullptr;
  ctx->client = nullptr;
  ctx->compositor = nullptr;
  ctx->subcompositor = nullptr;
  ctx->shm = nullptr;
  ctx->shell = nullptr;
  ctx->data_device_manager = nullptr;
  ctx->xdg_shell = nullptr;
  ctx->aura_shell = nullptr;
  ctx->viewporter = nullptr;
  ctx->linux_dmabuf = nullptr;
  ctx->keyboard_extension = nullptr;
  ctx->text_input_manager = nullptr;
  ctx->text_input_extension = nullptr;
  ctx->xdg_output_manager = nullptr;
  ctx->fractional_scale_manager = nullptr;
#ifdef GAMEPAD_SUPPORT
  ctx->gaming_input_manager = nullptr;
  ctx->gaming_seat = nullptr;
#endif
  ctx->display_event_source = nullptr;
  ctx->display_ready_event_source = nullptr;
  ctx->sigchld_event_source = nullptr;
  ctx->sigusr1_event_source = nullptr;
  ctx->wm_fd = -1;
  ctx->wayland_channel_fd = -1;
  ctx->virtwl_socket_fd = -1;
  ctx->virtwl_display_fd = -1;
  ctx->wayland_channel_event_source = nullptr;
  ctx->virtwl_socket_event_source = nullptr;
  ctx->vm_id = DEFAULT_VM_NAME;
  ctx->drm_device = nullptr;
  ctx->gbm = nullptr;
  ctx->xwayland = 0;
  ctx->xwayland_pid = -1;
  ctx->child_pid = -1;
  ctx->peer_pid = -1;
  ctx->xkb_context = nullptr;
  ctx->next_global_id = 1;
  ctx->connection = nullptr;
  ctx->connection_event_source = nullptr;
  ctx->xfixes_extension = nullptr;
  ctx->screen = nullptr;
  ctx->window = 0;
  ctx->host_focus_window = nullptr;
  ctx->needs_set_input_focus = 0;
  ctx->desired_scale = 1.0;
  ctx->scale = 1.0;
  ctx->virt_scale_x = 1.0;
  ctx->virt_scale_y = 1.0;
  ctx->xdg_scale_x = 1.0;
  ctx->xdg_scale_y = 1.0;
  ctx->application_id = nullptr;
  ctx->application_id_property_name = nullptr;
  ctx->exit_with_child = 1;
  ctx->sd_notify = nullptr;
  ctx->clipboard_manager = 0;
  ctx->frame_color = 0xffffffff;
  ctx->dark_frame_color = 0xff000000;
  ctx->support_damage_buffer = true;
  ctx->fullscreen_mode = ZAURA_SURFACE_FULLSCREEN_MODE_IMMERSIVE;
  ctx->allow_xwayland_emulate_screen_pos_size = false;
  ctx->ignore_stateless_toplevel_configure = false;
  ctx->only_client_can_exit_fullscreen = false;
  ctx->default_seat = nullptr;
  ctx->selection_window = XCB_WINDOW_NONE;
  ctx->selection_owner = XCB_WINDOW_NONE;
  ctx->selection_incremental_transfer = 0;
  ctx->selection_request.requestor = XCB_NONE;
  ctx->selection_request.property = XCB_ATOM_NONE;
  ctx->selection_timestamp = XCB_CURRENT_TIME;
  ctx->selection_data_device = nullptr;
  ctx->selection_data_offer = nullptr;
  ctx->selection_data_source = nullptr;
  ctx->selection_data_source_send_fd = -1;
  ctx->selection_send_event_source = nullptr;
  ctx->selection_property_reply = nullptr;
  ctx->selection_property_offset = 0;
  ctx->selection_event_source = nullptr;
  ctx->selection_data_offer_receive_fd = -1;
  ctx->selection_data_ack_pending = 0;
  for (unsigned i = 0; i < ARRAY_SIZE(ctx->atoms); i++) {
    const char* name = sl_context_atom_name(i);
    assert(name != nullptr);
    ctx->atoms[i].name = name;
  }
  ctx->timing = nullptr;
  ctx->trace_filename = nullptr;
#ifdef QUIRKS_SUPPORT
  new (&ctx->quirks) Quirks();
#endif
  ctx->enable_xshape = false;
  ctx->enable_x11_move_windows = false;
  ctx->trace_system = false;
  ctx->use_direct_scale = false;
  ctx->stable_scaling = false;
  ctx->frame_stats = nullptr;
  ctx->stats_timer_delay = 60 * 1000;
  ctx->viewport_resize = false;

  wl_list_init(&ctx->registries);
  wl_list_init(&ctx->globals);
  wl_list_init(&ctx->outputs);
  wl_list_init(&ctx->seats);
  wl_list_init(&ctx->windows);
  wl_list_init(&ctx->unpaired_windows);
  wl_list_init(&ctx->selection_data_source_send_pending);
#ifdef GAMEPAD_SUPPORT
  wl_list_init(&ctx->gamepads);
#endif
}

static int sl_handle_clipboard_event(int fd, uint32_t mask, void* data) {
  int rv;
  struct sl_context* ctx = (struct sl_context*)data;
  bool readable = false;
  bool hang_up = false;

  if (mask & WL_EVENT_READABLE)
    readable = true;
  if (mask & WL_EVENT_HANGUP)
    hang_up = true;

  rv = ctx->channel->handle_pipe(fd, readable, hang_up);
  if (rv) {
    LOG(ERROR) << "reading pipe failed with " << strerror(rv);
    return 0;
  }

  if (hang_up) {
    ctx->clipboard_event_source.reset();
    return 0;
  }

  return 1;
}

static int sl_handle_wayland_channel_event(int fd, uint32_t mask, void* data) {
  TRACE_EVENT("surface", "sl_handle_wayland_channel_event");
  struct sl_context* ctx = (struct sl_context*)data;
  struct WaylandSendReceive receive = {0};
  int pipe_read_fd = -1;
  enum WaylandChannelEvent event_type = WaylandChannelEvent::None;

  char fd_buffer[CMSG_LEN(sizeof(int) * WAYLAND_MAX_FDs)];
  struct msghdr msg = {0};
  struct iovec buffer_iov;
  ssize_t bytes;
  int rv;

  if (!(mask & WL_EVENT_READABLE)) {
    LOG(FATAL) << "Got error or hangup on virtwl ctx fd (mask " << mask
               << "), exiting";
    exit(EXIT_FAILURE);
  }

  receive.channel_fd = fd;
  rv = ctx->channel->handle_channel_event(event_type, receive, pipe_read_fd);
  if (rv) {
    close(ctx->virtwl_socket_fd);
    ctx->virtwl_socket_fd = -1;
    return 0;
  }

  if (event_type == WaylandChannelEvent::ReceiveAndProxy) {
    struct wl_event_loop* event_loop =
        wl_display_get_event_loop(ctx->host_display);

    ctx->clipboard_event_source.reset(
        wl_event_loop_add_fd(event_loop, pipe_read_fd, WL_EVENT_READABLE,
                             sl_handle_clipboard_event, ctx));
  } else if (event_type != WaylandChannelEvent::Receive) {
    return 1;
  }

  buffer_iov.iov_base = receive.data;
  buffer_iov.iov_len = receive.data_size;

  msg.msg_iov = &buffer_iov;
  msg.msg_iovlen = 1;
  msg.msg_control = fd_buffer;

  if (receive.num_fds) {
    struct cmsghdr* cmsg;

    // Need to set msg_controllen so CMSG_FIRSTHDR will return the first
    // cmsghdr. We copy every fd we just received from the ioctl into this
    // cmsghdr.
    msg.msg_controllen = sizeof(fd_buffer);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(receive.num_fds * sizeof(int));
    memcpy(CMSG_DATA(cmsg), receive.fds, receive.num_fds * sizeof(int));
    msg.msg_controllen = cmsg->cmsg_len;
  }

  bytes = sendmsg(ctx->virtwl_socket_fd, &msg, MSG_NOSIGNAL);
  errno_assert(bytes == static_cast<ssize_t>(receive.data_size));

  while (receive.num_fds--)
    close(receive.fds[receive.num_fds]);

  if (receive.data)
    free(receive.data);

  return 1;
}

static int sl_handle_virtwl_socket_event(int fd, uint32_t mask, void* data) {
  TRACE_EVENT("surface", "sl_handle_virtwl_socket_event");
  struct sl_context* ctx = (struct sl_context*)data;
  struct WaylandSendReceive send = {0};
  char fd_buffer[CMSG_LEN(sizeof(int) * WAYLAND_MAX_FDs)];
  uint8_t data_buffer[DEFAULT_BUFFER_SIZE];

  struct iovec buffer_iov;
  struct msghdr msg = {0};
  struct cmsghdr* cmsg;
  ssize_t bytes;
  int rv;

  if (!(mask & WL_EVENT_READABLE)) {
    LOG(FATAL) << "Got error or hangup on virtwl socket (mask " << mask
               << "), exiting";
    exit(EXIT_FAILURE);
  }

  buffer_iov.iov_base = data_buffer;
  buffer_iov.iov_len = ctx->channel->max_send_size();

  msg.msg_iov = &buffer_iov;
  msg.msg_iovlen = 1;
  msg.msg_control = fd_buffer;
  msg.msg_controllen = sizeof(fd_buffer);

  bytes = recvmsg(ctx->virtwl_socket_fd, &msg, 0);
  errno_assert(bytes > 0);

  // If there were any FDs recv'd by recvmsg, there will be some data in the
  // msg_control buffer. To get the FDs out we iterate all cmsghdr's within and
  // unpack the FDs if the cmsghdr type is SCM_RIGHTS.
  for (cmsg = msg.msg_controllen != 0 ? CMSG_FIRSTHDR(&msg) : nullptr; cmsg;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    size_t cmsg_fd_count;

    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
      continue;

    cmsg_fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);

    // fd_count will never exceed WAYLAND_MAX_FDs because the
    // control message buffer only allocates enough space for that many FDs.
    memcpy(&send.fds[send.num_fds], CMSG_DATA(cmsg),
           cmsg_fd_count * sizeof(int));
    send.num_fds += cmsg_fd_count;
  }

  send.channel_fd = ctx->wayland_channel_fd;
  send.data = data_buffer;
  send.data_size = bytes;

  rv = ctx->channel->send(send);
  errno_assert(!rv);

  while (send.num_fds--)
    close(send.fds[send.num_fds]);

  return 1;
}

wl_event_loop* sl_context_configure_event_loop(sl_context* ctx,
                                               WaylandChannel* channel,
                                               bool use_virtual_context) {
  assert(ctx);
  // caller must provide a wayland channel to use virtual context
  assert(!use_virtual_context || channel);

  wl_display* host_display = wl_display_create();
  if (!host_display) {
    LOG(ERROR) << "failed to create host wayland display.";
    return nullptr;
  }
  // libwayland ensures event_loop can be retrieved from a valid display, but
  // assert such in case that assumption ever changes.
  wl_event_loop* event_loop = wl_display_get_event_loop(host_display);
  assert(event_loop);

  int wayland_channel_fd = -1;
  if (channel && use_virtual_context) {
    // WARNING: It's critical that we never call wl_display_roundtrip
    // as we're not spawning a new thread to handle forwarding. Calling
    // wl_display_roundtrip will cause a deadlock.
    int ret;

    ret = channel->create_context(wayland_channel_fd);
    if (ret) {
      LOG(ERROR) << "failed to create virtwl context: " << strerror(-ret);
      return nullptr;
    }

    // Connection to virtwl channel.
    int vws[2];
    ret = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, vws);
    if (ret) {
      LOG(ERROR) << "failed to create wayland channel socket pair: "
                 << strerror(errno);
      return nullptr;
    }

    ctx->virtwl_socket_fd = vws[0];
    ctx->virtwl_display_fd = vws[1];

    ctx->virtwl_socket_event_source.reset(wl_event_loop_add_fd(
        event_loop, ctx->virtwl_socket_fd, WL_EVENT_READABLE,
        sl_handle_virtwl_socket_event, ctx));
    ctx->wayland_channel_event_source.reset(
        wl_event_loop_add_fd(event_loop, wayland_channel_fd, WL_EVENT_READABLE,
                             sl_handle_wayland_channel_event, ctx));
  }

  ctx->host_display = host_display;
  ctx->channel = channel;
  ctx->wayland_channel_fd = wayland_channel_fd;
  return event_loop;
}

sl_window* sl_context_lookup_window_for_surface(struct sl_context* ctx,
                                                wl_resource* resource) {
  sl_window* surface_window = nullptr;
  sl_window* window;

  wl_list_for_each(window, &ctx->windows, link) {
    if (window->host_surface_id == wl_resource_get_id(resource)) {
      surface_window = window;
      break;
    }
  }

  return surface_window;
}
