// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"            // NOLINT(build/include_directory)
#include "sommelier-transform.h"  // NOLINT(build/include_directory)

#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <wayland-client.h>

#include "aura-shell-client-protocol.h"  // NOLINT(build/include_directory)
#include "xdg-output-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)

#define MAX_OUTPUT_SCALE 2

#define INCH_IN_MM 25.4

// Legacy X11 applications use DPI to decide on their scale. This value is what
// the convention for a "normal" scale is. One way to verify the convention is
// to note the DPI of a typical monitor circa ~2005, i.e. 20" 1080p.
#define DEFACTO_DPI 96

double sl_output_aura_scale_factor_to_double(int scale_factor) {
  // Aura scale factor is an enum that for all currently know values
  // is a scale value multipled by 1000. For example, enum value for
  // 1.25 scale factor is 1250.
  return scale_factor / 1000.0;
}

int dpi_to_physical_mm(double dpi, int px) {
  return px * (INCH_IN_MM / dpi);
}

void sl_output_apply_rotation(
    int transform, int width, int height, int* out_width, int* out_height) {
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      *out_width = width;
      *out_height = height;
      break;

    default:
      *out_width = height;
      *out_height = width;
      break;
  }
}

void sl_output_get_host_output_state(struct sl_host_output* host,
                                     int* scale,
                                     int* physical_width,
                                     int* physical_height,
                                     int* width,
                                     int* height) {
  // The user's chosen zoom level.
  double current_scale =
      sl_output_aura_scale_factor_to_double(host->current_scale);

  // The scale applied to a screen at the default zoom. I.e. this value
  // determines the meaning of "100%" zoom, and how zoom relates to the
  // apparent resolution:
  //
  //    apparent_res = native_res / device_scale_factor * current_scale
  //
  // e.g.: On a device with a DSF of 2.0, 80% zoom really means "apply 1.6x
  // scale", and 50% zoom would give you an apparent resolution equal to the
  // native one.
  double device_scale_factor =
      sl_output_aura_scale_factor_to_double(host->device_scale_factor);

  // Optimistically, we will try to apply the scale that the user chose.
  // Failing that, we will use the scale set for this wl_output.
  double applied_scale = device_scale_factor * current_scale;
  if (!host->ctx->aura_shell) {
    applied_scale = host->scale_factor;
  }

  int target_dpi = DEFACTO_DPI;
  if (host->ctx->xwayland) {
    // For X11, we must fix the scale to be 1 (since X apps typically can't
    // handle scaling). As a result, we adjust the resolution (based on the
    // scale we want to apply and sommelier's configuration) and the physical
    // dimensions (based on what DPI we want the applications to use). E.g.:
    //  - Device scale is 1.25x, with 1920x1080 resolution on a 295mm by 165mm
    //    screen.
    //  - User chosen zoom is 130%
    //  - Sommelier is scaled to 0.5 (a.k.a low density). Since ctx->scale also
    //    has the device scale, it will be 0.625 (i.e. 0.5 * 1.25).
    //  - We want the DPI to be 120 (i.e. 96 * 1.25)
    //     - Meaning 0.21 mm/px
    //  - We report resolution 738x415 (1920x1080 * 0.5 / 1.3)
    //  - We report dimensions 155mm by 87mm (738x415 * 0.21)
    // This is mostly expected, another way of thinking about them is that zoom
    // and scale modify the application's understanding of length:
    //  - Increasing the zoom makes lengths appear longer (i.e. fewer mm to work
    //    with over the same real length).
    //  - Scaling the screen does the inverse.
    if (scale)
      *scale = 1;
    *width = host->width * host->ctx->scale / applied_scale;
    *height = host->height * host->ctx->scale / applied_scale;

    target_dpi = DEFACTO_DPI * device_scale_factor;
    *physical_width = dpi_to_physical_mm(target_dpi, *width);
    *physical_height = dpi_to_physical_mm(target_dpi, *height);
  } else {
    // For wayland, we directly apply the scale which combines the user's chosen
    // preference (from aura) and the scale which this sommelier was configured
    // for (i.e. based on ctx->scale, which comes from the env/cmd line).
    //
    // See above comment: ctx->scale already has the device_scale_factor in it,
    // so this maths actually looks like:
    //
    //              applied / ctx->scale
    //      = (current*DSF) / (config*DSF)
    //      =       current / config
    //
    // E.g. if we configured sommelier to scale everything 0.5x, and the user
    // has chosen 130% zoom, we are applying 2.6x scale factor.
    int s = MIN(ceil(applied_scale / host->ctx->scale), MAX_OUTPUT_SCALE);

    if (scale)
      *scale = s;
    *physical_width = host->physical_width;
    *physical_height = host->physical_height;
    *width = host->width * host->ctx->scale * s / applied_scale;
    *height = host->height * host->ctx->scale * s / applied_scale;
    target_dpi = (*width * INCH_IN_MM) / *physical_width;
  }

  if (host->ctx->dpi.size) {
    int adjusted_dpi = *(reinterpret_cast<int*>(host->ctx->dpi.data));

    // Choose the DPI bucket which is closest to the target DPI which we
    // calculated above.
    int* dpi;
    sl_array_for_each(dpi, &host->ctx->dpi) {
      if (abs(*dpi - target_dpi) < abs(adjusted_dpi - target_dpi))
        adjusted_dpi = *dpi;
    }

    *physical_width = dpi_to_physical_mm(adjusted_dpi, *width);
    *physical_height = dpi_to_physical_mm(adjusted_dpi, *height);
  }
}

void sl_output_get_logical_dimensions(struct sl_host_output* host,
                                      bool rotated,
                                      int32_t* width,
                                      int32_t* height) {
  if (rotated) {
    // Pass the dimensions as is (it could be rotated)
    *width = host->logical_width;
    *height = host->logical_height;
  } else {
    // The transform here indicates how a window image will be
    // rotated when composited. The incoming surface from the
    // application will NOT have its dimensions rotated.
    // For this reason, in order to calculate the scale factors
    // for direct scale, we will need the non rotated logical
    // dimensions.

    sl_output_apply_rotation(host->transform, host->logical_width,
                             host->logical_height, width, height);
  }
}

void sl_output_init_dimensions_direct(struct sl_host_output* host,
                                      int* out_scale,
                                      int* out_physical_width,
                                      int* out_physical_height,
                                      int* out_width,
                                      int* out_height) {
  int32_t virtual_width = host->width;
  int32_t virtual_height = host->height;

  // This requires xdg_output_manager, it is assumed that it will be
  // available and we will have an appropriate set of logical dimensions
  // for this particular output.
  assert(host->ctx->viewporter);
  assert(host->ctx->xdg_output_manager);

  // The virtual width/height is computed by this function here based
  // on the physical width/height
  sl_transform_output_dimensions(host->ctx, &virtual_width, &virtual_height);

  host->virt_scale_x = static_cast<double>(virtual_width) / host->width;
  host->virt_scale_y = static_cast<double>(virtual_height) / host->height;

  *out_width = virtual_width;
  *out_height = virtual_height;

  // Force the scale to 1
  //
  // This is reported to the guest through the wl_output protocol.
  // This value will signal by how much a compositor will upscale
  // all buffers by (1 is no scale).
  *out_scale = 1;

  // The physical dimensions (in mm) are the same, regardless
  // of the provided scale factor.
  *out_physical_width = host->physical_width;
  *out_physical_height = host->physical_height;

  // Retrieve the logical dimensions
  int32_t logical_width, logical_height;

  sl_output_get_logical_dimensions(host, /*rotated=*/false, &logical_width,
                                   &logical_height);

  // We want to be able to transform from virtual to XDG logical
  // coordinates
  // Virt to XDG -> div
  // XDG to Virt -> mul
  host->xdg_scale_x =
      static_cast<double>(virtual_width) / static_cast<double>(logical_width);
  host->xdg_scale_y =
      static_cast<double>(virtual_height) / static_cast<double>(logical_height);

  if (host->internal) {
    host->ctx->virt_scale_x = host->virt_scale_x;
    host->ctx->virt_scale_y = host->virt_scale_y;
    host->ctx->xdg_scale_x = host->xdg_scale_x;
    host->ctx->xdg_scale_y = host->xdg_scale_y;
  }
}

void sl_output_get_dimensions_original(struct sl_host_output* host,
                                       int* out_scale,
                                       int* out_physical_width,
                                       int* out_physical_height,
                                       int* out_width,
                                       int* out_height) {
  int scale;
  int physical_width;
  int physical_height;
  int width;
  int height;

  sl_output_get_host_output_state(host, &scale, &physical_width,
                                  &physical_height, &width, &height);

  // Use density of internal display for all Xwayland outputs. X11 clients
  // typically lack support for dynamically changing density so it's
  // preferred to always use the density of the internal display.
  if (host->ctx->xwayland) {
    for (auto output : host->ctx->host_outputs) {
      if (output->internal) {
        int internal_width;
        int internal_height;

        sl_output_get_host_output_state(output, nullptr, &physical_width,
                                        &physical_height, &internal_width,
                                        &internal_height);

        physical_width = (physical_width * width) / internal_width;
        physical_height = (physical_height * height) / internal_height;
        break;
      }
    }
  }

  *out_scale = scale;
  *out_physical_width = physical_width;
  *out_physical_height = physical_height;
  *out_width = width;
  *out_height = height;
}

// Recalculates the virt_x coordinates of outputs when an output is
// add/removed/changed.
void sl_output_update_output_x(struct sl_context* ctx) {
  // Outputs are positioned in a line from left to right based off their x
  // position.
  int next_output_x = 0;
  for (auto output : ctx->host_outputs) {
    // Update the value and mark for sending.
    if (output->virt_x != next_output_x) {
      output->virt_x = next_output_x;
      output->needs_update = true;
    }
    next_output_x += output->virt_rotated_width;
  }
}

struct sl_host_output* sl_infer_output_for_host_position(struct sl_context* ctx,
                                                         int32_t host_x,
                                                         int32_t host_y) {
  struct sl_host_output* closest = nullptr;
  int32_t closest_distance = INT32_MAX;

  // Return the output containing, or closest to, the query X/Y coordinates
  // in host logical space. "Closest" considers Manhattan distance.
  for (auto output : ctx->host_outputs) {
    if (!closest) {
      closest = output;
    }
    int32_t x_distance;
    if (host_x < output->x) {
      // Query point is left of the output
      x_distance = output->x - host_x;
    } else if (host_x < output->x + output->width) {
      // Query point is inside the output (on X axis)
      x_distance = 0;
    } else {
      // Query point is right of the output
      x_distance = host_x - (output->x + output->width);
    }
    int32_t y_distance;
    if (host_y < output->y) {
      // Query point is above the output
      y_distance = output->y - host_y;
    } else if (host_y < output->y + output->height) {
      // Query point is inside the output (on Y axis)
      y_distance = 0;
    } else {
      // Query point is below the output
      y_distance = host_y - (output->y + output->height);
    }
    if (x_distance + y_distance < closest_distance) {
      closest = output;
      closest_distance = x_distance + y_distance;
      if (closest_distance == 0) {
        break;
      }
    }
  }
  return closest;
}

struct sl_host_output* sl_infer_output_for_guest_position(
    struct sl_context* ctx, int32_t virt_x, int32_t virt_y) {
  struct sl_host_output* first = nullptr;
  struct sl_host_output* last = nullptr;

  // Return the output containing the query X coordinate (in virtual space).
  // Since outputs are placed in a horizontal line in virtual space, we can
  // ignore the Y coordinate entirely.
  for (auto output : ctx->host_outputs) {
    if (!first) {
      first = output;
    }
    last = output;
    if (virt_x >= output->virt_x && virt_x < output->virt_x + output->width) {
      return output;
    }
  }

  // The query X coordinate is out of bounds, so return the "nearest" output.
  if (first && virt_x < first->virt_x) {
    return first;
  }
  return last;
}

void sl_output_calculate_virtual_dimensions(struct sl_host_output* host) {
  int scale;
  int virt_physical_width;
  int virt_physical_height;
  int virt_width;
  int virt_height;

  if (host->ctx->use_direct_scale) {
    sl_output_init_dimensions_direct(host, &scale, &virt_physical_width,
                                     &virt_physical_height, &virt_width,
                                     &virt_height);
  } else {
    sl_output_get_dimensions_original(host, &scale, &virt_physical_width,
                                      &virt_physical_height, &virt_width,
                                      &virt_height);
  }

  host->scale_factor = scale;
  host->virt_width = virt_width;
  host->virt_height = virt_height;
  host->virt_physical_width = virt_physical_width;
  host->virt_physical_height = virt_physical_height;

  sl_output_apply_rotation(host->transform, virt_width, virt_height,
                           &host->virt_rotated_width,
                           &host->virt_rotated_height);
  host->needs_update = true;
}

// Function which pushes the state of an output to the client.
void sl_output_send_host_output_state(struct sl_host_output* host) {
  // Could be more granular, but the current implementation means that if one
  // value changes, everything should be impacted.
  if (host->needs_update) {
    wl_output_send_geometry(host->resource, host->virt_x, 0,
                            host->virt_physical_width,
                            host->virt_physical_height, host->subpixel,
                            host->make, host->model, host->transform);
    wl_output_send_mode(host->resource, host->flags | WL_OUTPUT_MODE_CURRENT,
                        host->virt_width, host->virt_height, host->refresh);
    if (wl_resource_get_version(host->resource) >=
        WL_OUTPUT_SCALE_SINCE_VERSION)
      wl_output_send_scale(host->resource, host->scale_factor);
    if (wl_resource_get_version(host->resource) >= WL_OUTPUT_DONE_SINCE_VERSION)
      wl_output_send_done(host->resource);
    host->needs_update = false;
  }
}

static void sl_output_geometry(void* data,
                               struct wl_output* output,
                               int x,
                               int y,
                               int physical_width,
                               int physical_height,
                               int subpixel,
                               const char* make,
                               const char* model,
                               int transform) {
  void* result = wl_output_get_user_data(output);
  sl_host_output* host = static_cast<sl_host_output*>(result);

  host->x = x;
  host->y = y;
  host->physical_width = physical_width;
  host->physical_height = physical_height;
  host->subpixel = subpixel;
  free(host->model);
  host->model = strdup(model);
  free(host->make);
  host->make = strdup(make);
  host->transform = transform;
  auto pointer = std::find(host->ctx->host_outputs.begin(),
                           host->ctx->host_outputs.end(), host);
  assert(pointer != host->ctx->host_outputs.end());
  // host_outputs is sorted by x. Delete then re-insert at the correct
  // position.
  host->ctx->host_outputs.erase(pointer);
  // Insert at the end by default. If insert_at is not set in the loop,
  // hosts's x is larger than all the ones in the list currently.
  auto insert_at = host->ctx->host_outputs.end();
  for (auto it = host->ctx->host_outputs.begin();
       it != host->ctx->host_outputs.end(); ++it) {
    if ((*it)->x > host->x) {
      insert_at = it;
      break;
    }
  }
  host->ctx->host_outputs.insert(insert_at, host);
}

static void sl_output_mode(void* data,
                           struct wl_output* output,
                           uint32_t flags,
                           int width,
                           int height,
                           int refresh) {
  struct sl_host_output* host =
      static_cast<sl_host_output*>(wl_output_get_user_data(output));

  host->flags = flags;
  host->width = width;
  host->height = height;
  host->refresh = refresh;
  host->needs_update = true;
}

static void sl_output_done(void* data, struct wl_output* output) {
  struct sl_host_output* host =
      static_cast<sl_host_output*>(wl_output_get_user_data(output));

  // Early out if scale is expected but not yet know.
  if (host->expecting_scale)
    return;

  // Recalculate according to any information that's been modified.
  sl_output_calculate_virtual_dimensions(host);
  // Shift all outputs that are to the right of host to the right if needed.
  sl_output_update_output_x(host->ctx);
  sl_output_send_host_output_state(host);

  // Expect scale if aura output exists.
  if (host->aura_output)
    host->expecting_scale = 1;
}

static void sl_output_scale(void* data,
                            struct wl_output* output,
                            int32_t scale_factor) {
  struct sl_host_output* host =
      static_cast<sl_host_output*>(wl_output_get_user_data(output));

  host->scale_factor = scale_factor;
}

static const struct wl_output_listener sl_output_listener = {
    sl_output_geometry, sl_output_mode, sl_output_done, sl_output_scale};

static void sl_aura_output_scale(void* data,
                                 struct zaura_output* output,
                                 uint32_t flags,
                                 uint32_t scale) {
  struct sl_host_output* host =
      static_cast<sl_host_output*>(zaura_output_get_user_data(output));

  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT)
    host->current_scale = scale;
  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED)
    host->preferred_scale = scale;

  host->expecting_scale = 0;
}

static void sl_aura_output_connection(void* data,
                                      struct zaura_output* output,
                                      uint32_t connection) {
  struct sl_host_output* host =
      static_cast<sl_host_output*>(zaura_output_get_user_data(output));

  host->internal = connection == ZAURA_OUTPUT_CONNECTION_TYPE_INTERNAL;
}

static void sl_aura_output_device_scale_factor(void* data,
                                               struct zaura_output* output,
                                               uint32_t device_scale_factor) {
  struct sl_host_output* host =
      static_cast<sl_host_output*>(zaura_output_get_user_data(output));

  host->device_scale_factor = device_scale_factor;
}

static const struct zaura_output_listener sl_aura_output_listener = {
    sl_aura_output_scale, sl_aura_output_connection,
    sl_aura_output_device_scale_factor, /*insets=*/DoNothing,
    /*logical_transform=*/DoNothing};

static void sl_destroy_host_output(struct wl_resource* resource) {
  struct sl_host_output* host =
      static_cast<sl_host_output*>(wl_resource_get_user_data(resource));

  if (host->aura_output)
    zaura_output_destroy(host->aura_output);
  if (wl_output_get_version(host->proxy) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
    wl_output_release(host->proxy);
  } else {
    wl_output_destroy(host->proxy);
  }
  wl_resource_set_user_data(resource, nullptr);
  auto pointer = std::find(host->ctx->host_outputs.begin(),
                           host->ctx->host_outputs.end(), host);
  assert(pointer != host->ctx->host_outputs.end());
  host->ctx->host_outputs.erase(pointer);
  free(host->make);
  free(host->model);
  // Shift all outputs to the right of the deleted output to the left.
  sl_output_update_output_x(host->ctx);
  delete host;
}

static void sl_xdg_output_logical_position(
    void* data, struct zxdg_output_v1* zxdg_output_v1, int32_t x, int32_t y) {
  struct sl_host_output* host = static_cast<sl_host_output*>(
      zxdg_output_v1_get_user_data(zxdg_output_v1));
  host->logical_y = y;
  host->logical_x = x;
}

static void sl_xdg_output_logical_size(void* data,
                                       struct zxdg_output_v1* zxdg_output_v1,
                                       int32_t width,
                                       int32_t height) {
  struct sl_host_output* host = static_cast<sl_host_output*>(
      zxdg_output_v1_get_user_data(zxdg_output_v1));

  host->logical_width = width;
  host->logical_height = height;

  host->expecting_logical_size = false;
}

static const struct zxdg_output_v1_listener sl_xdg_output_listener = {
    sl_xdg_output_logical_position, sl_xdg_output_logical_size,
    /*done=*/DoNothing,
    /*name=*/DoNothing, /*desc=*/DoNothing};

static void sl_bind_host_output(struct wl_client* client,
                                void* data,
                                uint32_t version,
                                uint32_t id) {
  struct sl_output* output = (struct sl_output*)data;
  struct sl_context* ctx = output->ctx;
  struct sl_host_output* host = new sl_host_output();
  host->ctx = ctx;
  host->resource = wl_resource_create(client, &wl_output_interface,
                                      MIN(version, output->version), id);
  wl_resource_set_implementation(host->resource, nullptr, host,
                                 sl_destroy_host_output);
  host->proxy = static_cast<wl_output*>(wl_registry_bind(
      wl_display_get_registry(ctx->display), output->id, &wl_output_interface,
      wl_resource_get_version(host->resource)));
  wl_output_add_listener(host->proxy, &sl_output_listener, host);
  output->host_output = host;
  host->aura_output = nullptr;
  // We assume that first output is internal by default.
  host->internal = ctx->host_outputs.empty();
  // We'll always need to forward this information.
  host->needs_update = true;
  host->x = 0;
  host->y = 0;
  host->virt_x = 0;
  host->virt_y = 0;
  host->logical_x = 0;
  host->logical_y = 0;
  host->physical_width = 0;
  host->physical_height = 0;
  host->virt_physical_width = 0;
  host->virt_physical_height = 0;
  host->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
  host->make = strdup("unknown");
  host->model = strdup("unknown");
  host->transform = WL_OUTPUT_TRANSFORM_NORMAL;
  host->flags = 0;
  host->width = 1024;
  host->height = 768;
  host->virt_width = 1024;
  host->virt_height = 768;
  host->virt_rotated_width = 0;
  host->virt_rotated_height = 0;
  host->logical_width = 1024;
  host->logical_height = 768;
  host->refresh = 60000;
  host->scale_factor = 1;
  host->current_scale = 1000;
  host->preferred_scale = 1000;
  host->device_scale_factor = 1000;
  host->expecting_scale = 0;
  host->expecting_logical_size = false;
  ctx->host_outputs.push_back(host);
  if (ctx->aura_shell) {
    host->expecting_scale = 1;
    host->internal = 0;
    host->aura_output =
        zaura_shell_get_aura_output(ctx->aura_shell->internal, host->proxy);
    zaura_output_add_listener(host->aura_output, &sl_aura_output_listener,
                              host);
  }

  if (ctx->xdg_output_manager) {
    host->expecting_logical_size = true;
    host->zxdg_output = zxdg_output_manager_v1_get_xdg_output(
        ctx->xdg_output_manager->internal, host->proxy);
    zxdg_output_v1_add_listener(host->zxdg_output, &sl_xdg_output_listener,
                                host);
  }
}

struct sl_global* sl_output_global_create(struct sl_output* output) {
  return sl_global_create(output->ctx, &wl_output_interface, output->version,
                          output, sl_bind_host_output);
}
