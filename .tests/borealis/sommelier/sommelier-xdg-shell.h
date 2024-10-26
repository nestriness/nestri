// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_XDG_SHELL_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_XDG_SHELL_H_

#include <wayland-server.h>

#include "sommelier-ctx.h"              // NOLINT(build/include_directory)
#include "sommelier-util.h"             // NOLINT(build/include_directory)
#include "xdg-shell-client-protocol.h"  // NOLINT(build/include_directory)

#define SL_XDG_SHELL_MAX_VERSION 3u

struct sl_host_xdg_shell {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct xdg_wm_base* proxy;
};
MAP_STRUCTS(xdg_wm_base, sl_host_xdg_shell);

struct sl_host_xdg_surface {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct xdg_surface* proxy;
  struct sl_host_surface* originator;
};
MAP_STRUCTS(xdg_surface, sl_host_xdg_surface);

struct sl_host_xdg_toplevel {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct xdg_toplevel* proxy;
  struct sl_host_xdg_surface* originator;
};
MAP_STRUCTS(xdg_toplevel, sl_host_xdg_toplevel);

struct sl_host_xdg_popup {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct xdg_popup* proxy;
  struct sl_host_xdg_surface* originator;
};
MAP_STRUCTS(xdg_popup, sl_host_xdg_popup);

struct sl_host_xdg_positioner {
  struct sl_context* ctx;
  struct wl_resource* resource;
  struct xdg_positioner* proxy;
};
MAP_STRUCTS(xdg_positioner, sl_host_xdg_positioner);

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_XDG_SHELL_H_
