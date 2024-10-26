// Copyright 2018 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"            // NOLINT(build/include_directory)
#include "sommelier-logging.h"    // NOLINT(build/include_directory)
#include "sommelier-transform.h"  // NOLINT(build/include_directory)

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wayland-client.h>

struct sl_host_data_device_manager {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_data_device_manager* proxy;
};

struct sl_host_data_device {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_data_device* proxy;
  struct sl_host_surface* focus_surface;
};
MAP_STRUCTS(wl_data_device, sl_host_data_device);

struct sl_host_data_source {
  struct wl_resource* resource;
  struct wl_data_source* proxy;
};
MAP_STRUCTS(wl_data_source, sl_host_data_source);

struct sl_host_data_offer {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct wl_data_offer* proxy;
};
MAP_STRUCTS(wl_data_offer, sl_host_data_offer);

struct sl_data_transfer {
  int read_fd;
  int write_fd;
  size_t offset;
  size_t bytes_left;
  uint8_t data[DEFAULT_BUFFER_SIZE];
  // Flag to temporarily track if we've just finished writing. This is used to
  // ignore the first WL_EVENT_HANGUP after a write since we seem to spuriously
  // get WL_EVENT_HANGUP when the socket isn't closed and can still be used
  // after a write.
  bool written;
  std::unique_ptr<struct wl_event_source> read_event_source;
  std::unique_ptr<struct wl_event_source> write_event_source;
};

static void sl_data_transfer_destroy(struct sl_data_transfer* transfer) {
  int read_fd = transfer->read_fd;
  int write_fd = transfer->write_fd;

  // default deleter of wl_event_source removes the sources from their
  // associated wl_event_loop, but we still want to close the fd. Ordered this
  // way because we don't want any additional events associated with the close()
  // calls to end up a the wl_event_loop.
  delete transfer;

  close(read_fd);
  close(write_fd);
}

static int sl_handle_data_transfer_read(int fd, uint32_t mask, void* data) {
  struct sl_data_transfer* transfer = (struct sl_data_transfer*)data;
  if ((mask & WL_EVENT_READABLE) == 0) {
    assert(mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR));

    // Epoll (and therefore wl_event_loop) will notify listeners of errors and
    // hangups even if all other events are disabled. Therefore, at this point,
    // we don't know whether we didn't get a readable event because the fd has
    // been exhausted, or because we aren't in the reading state and weren't
    // listening for one. We can make the distinction be checking if there's any
    // data left in the buffer. If there is, then we are just waiting for the
    // writing to finish, otherwise end the transfer.

    // In the case of an error, where there is not likely to be any more data to
    // read, we still want to wait for any data we did get to be written out.
    if (!transfer->bytes_left && !transfer->written) {
      sl_data_transfer_destroy(transfer);
    } else if (transfer->written) {
      transfer->written = false;
    }
    return 0;
  }

  // At this point we must be in the reading state.
  assert(!transfer->bytes_left);

  transfer->bytes_left =
      read(transfer->read_fd, transfer->data, sizeof(transfer->data));
  if (transfer->bytes_left > 0) {
    transfer->offset = 0;
    // There may still be data to read from the event source, but we have no
    // room in our buffer so move to the writing state.
    transfer->written = false;
    wl_event_source_fd_update(transfer->read_event_source.get(), 0);
    wl_event_source_fd_update(transfer->write_event_source.get(),
                              WL_EVENT_WRITABLE);
  } else {
    // On a read error or EOF, end the transfer.
    sl_data_transfer_destroy(transfer);
  }

  return 0;
}

static int sl_handle_data_transfer_write(int fd, uint32_t mask, void* data) {
  struct sl_data_transfer* transfer = (struct sl_data_transfer*)data;
  int rv;

  // If we receive a HANGUP or ERROR event on the write source then there is no
  // point in continuing the transfer. We could still read more data, but we
  // couldn't send it to the recipient, so just destroy the transfer now.
  if ((mask & WL_EVENT_WRITABLE) == 0) {
    assert(mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR));
    sl_data_transfer_destroy(transfer);
    return 0;
  }

  // At this point we must be in the writing state.
  assert(transfer->bytes_left);

  rv = write(transfer->write_fd, transfer->data + transfer->offset,
             transfer->bytes_left);

  if (rv < 0) {
    // On a write error, end the transfer.
    sl_data_transfer_destroy(transfer);
  } else {
    assert(rv <= static_cast<int>(transfer->bytes_left));
    transfer->bytes_left -= rv;
    transfer->offset += rv;
  }

  if (!transfer->bytes_left) {
    // If all data has been written, move back to the reading state.
    transfer->written = true;
    wl_event_source_fd_update(transfer->write_event_source.get(), 0);
    wl_event_source_fd_update(transfer->read_event_source.get(),
                              WL_EVENT_READABLE);
  }

  // If there is still data left, continue in the writing state.
  return 0;
}

static void sl_data_transfer_create(struct wl_event_loop* event_loop,
                                    int read_fd,
                                    int write_fd) {
  int flags;
  int rv;

  flags = fcntl(write_fd, F_GETFL, 0);
  rv = fcntl(write_fd, F_SETFL, flags | O_NONBLOCK);
  assert(!rv);
  UNUSED(rv);

  // Start out the transfer in the reading state.
  struct sl_data_transfer* transfer = new sl_data_transfer;
  assert(transfer);
  transfer->read_fd = read_fd;
  transfer->write_fd = write_fd;
  transfer->offset = 0;
  transfer->bytes_left = 0;
  transfer->written = false;
  memset(transfer->data, 0, DEFAULT_BUFFER_SIZE);
  transfer->read_event_source.reset(
      wl_event_loop_add_fd(event_loop, read_fd, WL_EVENT_READABLE,
                           sl_handle_data_transfer_read, transfer));
  transfer->write_event_source.reset(wl_event_loop_add_fd(
      event_loop, write_fd, 0, sl_handle_data_transfer_write, transfer));
}

static void sl_data_offer_receive(struct wl_client* client,
                                  struct wl_resource* resource,
                                  const char* mime_type,
                                  int32_t fd) {
  struct sl_host_data_offer* host =
      static_cast<sl_host_data_offer*>(wl_resource_get_user_data(resource));

  if (host->ctx->channel == nullptr) {
    // Running in noop mode, without virtualization.
    wl_data_offer_receive(host->proxy, mime_type, fd);
    close(fd);
    return;
  }

  int pipe_fd, rv;
  rv = host->ctx->channel->create_pipe(pipe_fd);
  if (rv) {
    LOG(ERROR) << "failed to create virtwl pipe: " << strerror(-rv);
    close(fd);
    return;
  }

  sl_data_transfer_create(wl_display_get_event_loop(host->ctx->host_display),
                          pipe_fd, fd);

  wl_data_offer_receive(host->proxy, mime_type, pipe_fd);
}

static void sl_data_offer_destroy(struct wl_client* client,
                                  struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct wl_data_offer_interface sl_data_offer_implementation = {
    ForwardRequest<wl_data_offer_accept>, sl_data_offer_receive,
    sl_data_offer_destroy, ForwardRequest<wl_data_offer_finish>,
    ForwardRequest<wl_data_offer_set_actions>};

static void sl_data_offer_offer(void* data,
                                struct wl_data_offer* data_offer,
                                const char* mime_type) {
  struct sl_host_data_offer* host =
      static_cast<sl_host_data_offer*>(wl_data_offer_get_user_data(data_offer));

  wl_data_offer_send_offer(host->resource, mime_type);
}

static void sl_data_offer_source_actions(void* data,
                                         struct wl_data_offer* data_offer,
                                         uint32_t source_actions) {
  struct sl_host_data_offer* host =
      static_cast<sl_host_data_offer*>(wl_data_offer_get_user_data(data_offer));

  wl_data_offer_send_source_actions(host->resource, source_actions);
}

static void sl_data_offer_action(void* data,
                                 struct wl_data_offer* data_offer,
                                 uint32_t dnd_action) {
  struct sl_host_data_offer* host =
      static_cast<sl_host_data_offer*>(wl_data_offer_get_user_data(data_offer));

  wl_data_offer_send_action(host->resource, dnd_action);
}

static const struct wl_data_offer_listener sl_data_offer_listener = {
    sl_data_offer_offer, sl_data_offer_source_actions, sl_data_offer_action};

static void sl_destroy_host_data_offer(struct wl_resource* resource) {
  struct sl_host_data_offer* host =
      static_cast<sl_host_data_offer*>(wl_resource_get_user_data(resource));

  wl_data_offer_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_data_source_destroy(struct wl_client* client,
                                   struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct wl_data_source_interface sl_data_source_implementation = {
    ForwardRequest<wl_data_source_offer>, sl_data_source_destroy,
    ForwardRequest<wl_data_source_set_actions>};

static void sl_data_source_target(void* data,
                                  struct wl_data_source* data_source,
                                  const char* mime_type) {
  struct sl_host_data_source* host = static_cast<sl_host_data_source*>(
      wl_data_source_get_user_data(data_source));

  wl_data_source_send_target(host->resource, mime_type);
}

static void sl_data_source_send(void* data,
                                struct wl_data_source* data_source,
                                const char* mime_type,
                                int32_t fd) {
  struct sl_host_data_source* host = static_cast<sl_host_data_source*>(
      wl_data_source_get_user_data(data_source));

  wl_data_source_send_send(host->resource, mime_type, fd);
  close(fd);
}

static void sl_data_source_cancelled(void* data,
                                     struct wl_data_source* data_source) {
  struct sl_host_data_source* host = static_cast<sl_host_data_source*>(
      wl_data_source_get_user_data(data_source));

  wl_data_source_send_cancelled(host->resource);
}

void sl_data_source_dnd_drop_performed(void* data,
                                       struct wl_data_source* data_source) {
  struct sl_host_data_source* host = static_cast<sl_host_data_source*>(
      wl_data_source_get_user_data(data_source));

  wl_data_source_send_dnd_drop_performed(host->resource);
}

void sl_data_source_dnd_finished(void* data,
                                 struct wl_data_source* data_source) {
  struct sl_host_data_source* host = static_cast<sl_host_data_source*>(
      wl_data_source_get_user_data(data_source));

  wl_data_source_send_dnd_finished(host->resource);
}

void sl_data_source_actions(void* data,
                            struct wl_data_source* data_source,
                            uint32_t dnd_action) {
  struct sl_host_data_source* host = static_cast<sl_host_data_source*>(
      wl_data_source_get_user_data(data_source));

  wl_data_source_send_action(host->resource, dnd_action);
}

static const struct wl_data_source_listener sl_data_source_listener = {
    sl_data_source_target,       sl_data_source_send,
    sl_data_source_cancelled,    sl_data_source_dnd_drop_performed,
    sl_data_source_dnd_finished, sl_data_source_actions};

static void sl_destroy_host_data_source(struct wl_resource* resource) {
  struct sl_host_data_source* host =
      static_cast<sl_host_data_source*>(wl_resource_get_user_data(resource));

  wl_data_source_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_data_device_start_drag(struct wl_client* client,
                                      struct wl_resource* resource,
                                      struct wl_resource* source_resource,
                                      struct wl_resource* origin_resource,
                                      struct wl_resource* icon_resource,
                                      uint32_t serial) {
  struct sl_host_data_device* host =
      static_cast<sl_host_data_device*>(wl_resource_get_user_data(resource));
  struct sl_host_data_source* host_source =
      source_resource ? static_cast<sl_host_data_source*>(
                            wl_resource_get_user_data(source_resource))
                      : nullptr;
  struct sl_host_surface* host_origin =
      origin_resource ? static_cast<sl_host_surface*>(
                            wl_resource_get_user_data(origin_resource))
                      : nullptr;
  struct sl_host_surface* host_icon =
      icon_resource ? static_cast<sl_host_surface*>(
                          wl_resource_get_user_data(icon_resource))
                    : nullptr;
  host_icon->has_role = 1;

  wl_data_device_start_drag(host->proxy,
                            host_source ? host_source->proxy : nullptr,
                            host_origin ? host_origin->proxy : nullptr,
                            host_icon ? host_icon->proxy : nullptr, serial);
}

static void sl_data_device_release(struct wl_client* client,
                                   struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct wl_data_device_interface sl_data_device_implementation = {
    sl_data_device_start_drag,
    ForwardRequest<wl_data_device_set_selection, AllowNullResource::kYes>,
    sl_data_device_release};

static void sl_data_device_data_offer(void* data,
                                      struct wl_data_device* data_device,
                                      struct wl_data_offer* data_offer) {
  struct sl_host_data_device* host = static_cast<sl_host_data_device*>(
      wl_data_device_get_user_data(data_device));
  struct sl_host_data_offer* host_data_offer = new sl_host_data_offer();

  host_data_offer->ctx = host->ctx;
  host_data_offer->resource = wl_resource_create(
      wl_resource_get_client(host->resource), &wl_data_offer_interface,
      wl_resource_get_version(host->resource), 0);
  wl_resource_set_implementation(host_data_offer->resource,
                                 &sl_data_offer_implementation, host_data_offer,
                                 sl_destroy_host_data_offer);
  host_data_offer->proxy = data_offer;
  wl_data_offer_add_listener(host_data_offer->proxy, &sl_data_offer_listener,
                             host_data_offer);

  wl_data_device_send_data_offer(host->resource, host_data_offer->resource);
}

static void sl_data_device_enter(void* data,
                                 struct wl_data_device* data_device,
                                 uint32_t serial,
                                 struct wl_surface* surface,
                                 wl_fixed_t x,
                                 wl_fixed_t y,
                                 struct wl_data_offer* data_offer) {
  struct sl_host_data_device* host = static_cast<sl_host_data_device*>(
      wl_data_device_get_user_data(data_device));
  struct sl_host_surface* host_surface =
      static_cast<sl_host_surface*>(wl_surface_get_user_data(surface));
  struct sl_host_data_offer* host_data_offer =
      static_cast<sl_host_data_offer*>(wl_data_offer_get_user_data(data_offer));
  wl_fixed_t ix = x, iy = y;

  sl_transform_host_to_guest_fixed(host->ctx, host_surface, &ix, &iy);

  host->focus_surface = host_surface;

  wl_data_device_send_enter(host->resource, serial, host_surface->resource, ix,
                            iy, host_data_offer->resource);
}

static void sl_data_device_leave(void* data,
                                 struct wl_data_device* data_device) {
  struct sl_host_data_device* host = static_cast<sl_host_data_device*>(
      wl_data_device_get_user_data(data_device));

  host->focus_surface = nullptr;
  wl_data_device_send_leave(host->resource);
}

static void sl_data_device_motion(void* data,
                                  struct wl_data_device* data_device,
                                  uint32_t time,
                                  wl_fixed_t x,
                                  wl_fixed_t y) {
  struct sl_host_data_device* host = static_cast<sl_host_data_device*>(
      wl_data_device_get_user_data(data_device));
  wl_fixed_t ix = x, iy = y;

  sl_transform_host_to_guest_fixed(host->ctx, host->focus_surface, &ix, &iy);

  wl_data_device_send_motion(host->resource, time, ix, iy);
}

static void sl_data_device_drop(void* data,
                                struct wl_data_device* data_device) {
  struct sl_host_data_device* host = static_cast<sl_host_data_device*>(
      wl_data_device_get_user_data(data_device));

  host->focus_surface = nullptr;
  wl_data_device_send_drop(host->resource);
}

static void sl_data_device_selection(void* data,
                                     struct wl_data_device* data_device,
                                     struct wl_data_offer* data_offer) {
  struct sl_host_data_device* host = static_cast<sl_host_data_device*>(
      wl_data_device_get_user_data(data_device));
  struct wl_resource* data_offer_resource =
      data_offer ? static_cast<sl_host_data_offer*>(
                       wl_data_offer_get_user_data(data_offer))
                       ->resource
                 : nullptr;

  wl_data_device_send_selection(host->resource, data_offer_resource);
}

static const struct wl_data_device_listener sl_data_device_listener = {
    sl_data_device_data_offer, sl_data_device_enter, sl_data_device_leave,
    sl_data_device_motion,     sl_data_device_drop,  sl_data_device_selection};

static void sl_destroy_host_data_device(struct wl_resource* resource) {
  struct sl_host_data_device* host =
      static_cast<sl_host_data_device*>(wl_resource_get_user_data(resource));

  if (wl_data_device_get_version(host->proxy) >=
      WL_DATA_DEVICE_RELEASE_SINCE_VERSION) {
    wl_data_device_release(host->proxy);
  } else {
    wl_data_device_destroy(host->proxy);
  }
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_data_device_manager_create_data_source(
    struct wl_client* client, struct wl_resource* resource, uint32_t id) {
  struct sl_host_data_device_manager* host =
      static_cast<sl_host_data_device_manager*>(
          wl_resource_get_user_data(resource));
  struct sl_host_data_source* host_data_source = new sl_host_data_source();

  host_data_source->resource = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_data_source->resource,
                                 &sl_data_source_implementation,
                                 host_data_source, sl_destroy_host_data_source);
  host_data_source->proxy =
      wl_data_device_manager_create_data_source(host->proxy);
  wl_data_source_add_listener(host_data_source->proxy, &sl_data_source_listener,
                              host_data_source);
}

static void sl_data_device_manager_get_data_device(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* seat_resource) {
  struct sl_host_data_device_manager* host =
      static_cast<sl_host_data_device_manager*>(
          wl_resource_get_user_data(resource));
  struct sl_host_seat* host_seat =
      static_cast<sl_host_seat*>(wl_resource_get_user_data(seat_resource));
  struct sl_host_data_device* host_data_device = new sl_host_data_device();

  host_data_device->ctx = host->ctx;
  host_data_device->focus_surface = nullptr;
  host_data_device->resource = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_data_device->resource,
                                 &sl_data_device_implementation,
                                 host_data_device, sl_destroy_host_data_device);
  host_data_device->proxy =
      wl_data_device_manager_get_data_device(host->proxy, host_seat->proxy);
  wl_data_device_add_listener(host_data_device->proxy, &sl_data_device_listener,
                              host_data_device);
}

static const struct wl_data_device_manager_interface
    sl_data_device_manager_implementation = {
        sl_data_device_manager_create_data_source,
        sl_data_device_manager_get_data_device};

static void sl_destroy_host_data_device_manager(struct wl_resource* resource) {
  struct sl_host_data_device_manager* host =
      static_cast<sl_host_data_device_manager*>(
          wl_resource_get_user_data(resource));

  wl_data_device_manager_destroy(host->proxy);
  wl_resource_set_user_data(resource, nullptr);
  delete host;
}

static void sl_bind_host_data_device_manager(struct wl_client* client,
                                             void* data,
                                             uint32_t version,
                                             uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_data_device_manager* host = new sl_host_data_device_manager();
  host->ctx = ctx;
  host->resource =
      wl_resource_create(client, &wl_data_device_manager_interface,
                         MIN(version, ctx->data_device_manager->version), id);
  wl_resource_set_implementation(host->resource,
                                 &sl_data_device_manager_implementation, host,
                                 sl_destroy_host_data_device_manager);
  host->proxy = static_cast<wl_data_device_manager*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), ctx->data_device_manager->id,
      &wl_data_device_manager_interface, ctx->data_device_manager->version));
  wl_data_device_manager_set_user_data(host->proxy, host);
}

struct sl_global* sl_data_device_manager_global_create(struct sl_context* ctx) {
  return sl_global_create(ctx, &wl_data_device_manager_interface,
                          ctx->data_device_manager->version, ctx,
                          sl_bind_host_data_device_manager);
}
