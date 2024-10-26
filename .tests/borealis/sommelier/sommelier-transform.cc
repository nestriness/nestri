// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <type_traits>

#include "sommelier-tracing.h"    // NOLINT(build/include_directory)
#include "sommelier-transform.h"  // NOLINT(build/include_directory)

namespace {

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void stable_scale_host_to_guest(T* value, double scale) {
  *value = static_cast<T>(ceil(static_cast<double>(*value) * scale));
}

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void stable_scale_size_host_to_guest(T* value, double scale) {
  *value = static_cast<T>(floor(static_cast<double>(*value) * scale));
}

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void stable_scale_guest_to_host(T* value, double scale) {
  *value = static_cast<T>(floor(static_cast<double>(*value) / scale));
}

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void stable_scale_size_guest_to_host(T* value, double scale) {
  *value = static_cast<T>(ceil(static_cast<double>(*value) / scale));
}

}  // namespace

static void sl_transform_get_scale_factors(
    struct sl_context* ctx,
    const struct sl_host_surface* surface,
    double* scalex,
    double* scaley) {
  if (ctx->use_direct_scale && surface && surface->has_own_scale) {
    *scalex = surface->xdg_scale_x;
    *scaley = surface->xdg_scale_y;
  } else if (surface && surface->output) {
    *scalex = surface->output.get()->xdg_scale_x;
    *scaley = surface->output.get()->xdg_scale_y;
  } else {
    *scalex = ctx->xdg_scale_x;
    *scaley = ctx->xdg_scale_y;
  }
}

static double sl_transform_direct_axis_scale(struct sl_context* ctx,
                                             struct sl_host_surface* surface,
                                             uint32_t axis) {
  double scalex, scaley;

  sl_transform_get_scale_factors(ctx, surface, &scalex, &scaley);
  return (axis == 0) ? scaley : scalex;
}

static void sl_transform_direct_to_host_damage(
    struct sl_context* ctx,
    const struct sl_host_surface* surface,
    int64_t* x,
    int64_t* y,
    double scale_x,
    double scale_y) {
  if (ctx->stable_scaling) {
    stable_scale_guest_to_host(x, scale_x);
    stable_scale_guest_to_host(y, scale_y);
  } else {
    double x_whole = trunc(static_cast<double>(*x) / scale_x);
    double y_whole = trunc(static_cast<double>(*y) / scale_y);

    *x = static_cast<int64_t>(x_whole);
    *y = static_cast<int64_t>(y_whole);
  }
}

static void sl_transform_direct_to_guest_fixed(struct sl_context* ctx,
                                               struct sl_host_surface* surface,
                                               wl_fixed_t* coord,
                                               uint32_t axis) {
  double scale = sl_transform_direct_axis_scale(ctx, surface, axis);
  double result = wl_fixed_to_double(*coord);
  result *= scale;

  *coord = wl_fixed_from_double(result);
}

static void sl_transform_direct_to_guest_fixed(struct sl_context* ctx,
                                               struct sl_host_surface* surface,
                                               wl_fixed_t* x,
                                               wl_fixed_t* y) {
  double scale_x, scale_y;
  double result_x = wl_fixed_to_double(*x);
  double result_y = wl_fixed_to_double(*y);

  sl_transform_get_scale_factors(ctx, surface, &scale_x, &scale_y);

  result_x *= scale_x;
  result_y *= scale_y;

  *x = wl_fixed_from_double(result_x);
  *y = wl_fixed_from_double(result_y);
}

static void sl_transform_direct_to_host_fixed(struct sl_context* ctx,
                                              struct sl_host_surface* surface,
                                              wl_fixed_t* coord,
                                              uint32_t axis) {
  double scale = sl_transform_direct_axis_scale(ctx, surface, axis);
  double result = wl_fixed_to_double(*coord);

  result /= scale;

  *coord = wl_fixed_from_double(result);
}

static void sl_transform_direct_to_host_fixed(struct sl_context* ctx,
                                              struct sl_host_surface* surface,
                                              wl_fixed_t* x,
                                              wl_fixed_t* y) {
  double scale_x, scale_y;

  sl_transform_get_scale_factors(ctx, surface, &scale_x, &scale_y);

  double result_x = wl_fixed_to_double(*x);
  double result_y = wl_fixed_to_double(*y);

  result_x /= scale_x;
  result_y /= scale_y;

  *x = wl_fixed_from_double(result_x);
  *y = wl_fixed_from_double(result_y);
}

static void sl_transform_direct_to_guest(struct sl_context* ctx,
                                         struct sl_host_surface* surface,
                                         int32_t* x,
                                         int32_t* y) {
  double scale_x, scale_y;

  sl_transform_get_scale_factors(ctx, surface, &scale_x, &scale_y);

  if (ctx->stable_scaling) {
    stable_scale_host_to_guest(x, scale_x);
    stable_scale_host_to_guest(y, scale_y);
  } else {
    double input_x = static_cast<double>(*x) * scale_x;
    double input_y = static_cast<double>(*y) * scale_y;

    double x_whole = (surface && surface->scale_round_on_x) ? lround(input_x)
                                                            : trunc(input_x);

    double y_whole = (surface && surface->scale_round_on_y) ? lround(input_y)
                                                            : trunc(input_y);

    *x = static_cast<int32_t>(x_whole);
    *y = static_cast<int32_t>(y_whole);
  }
}

static void sl_transform_direct_to_host(struct sl_context* ctx,
                                        struct sl_host_surface* surface,
                                        int32_t* x,
                                        int32_t* y) {
  double scale_x, scale_y;

  sl_transform_get_scale_factors(ctx, surface, &scale_x, &scale_y);

  if (ctx->stable_scaling) {
    stable_scale_guest_to_host(x, scale_x);
    stable_scale_guest_to_host(y, scale_y);
  } else {
    double x_whole = trunc(static_cast<double>(*x) / scale_x);
    double y_whole = trunc(static_cast<double>(*y) / scale_y);

    *x = static_cast<int32_t>(x_whole);
    *y = static_cast<int32_t>(y_whole);
  }
}

bool sl_transform_viewport_scale(struct sl_context* ctx,
                                 struct sl_host_surface* surface,
                                 double contents_scale,
                                 int32_t* width,
                                 int32_t* height) {
  double scale = ctx->scale * contents_scale;

  // TODO(mrisaacb): It may be beneficial to skip the set_destination call
  // when the virtual and logical space match.
  bool do_viewport = true;

  if (surface && surface->window && surface->window->viewport_override) {
    *width = surface->window->viewport_width;
    *height = surface->window->viewport_height;
  } else if (ctx->use_direct_scale) {
    sl_transform_direct_to_host(ctx, surface, width, height);

    // For very small windows (in pixels), the resulting logical dimensions
    // could be 0, which will cause issues with the viewporter interface.
    //
    // In these cases, fix it up here by forcing the logical output
    // to be at least 1 pixel

    if (*width <= 0)
      *width = 1;

    if (*height <= 0)
      *height = 1;

  } else {
    if (ctx->stable_scaling) {
      stable_scale_size_guest_to_host(width, scale);
      stable_scale_size_guest_to_host(height, scale);
    } else {
      *width = ceil(*width / scale);
      *height = ceil(*height / scale);
    }
  }

  return do_viewport;
}

void sl_transform_damage_coord(struct sl_context* ctx,
                               const struct sl_host_surface* surface,
                               double buffer_scalex,
                               double buffer_scaley,
                               int64_t* x1,
                               int64_t* y1,
                               int64_t* x2,
                               int64_t* y2) {
  if (ctx->use_direct_scale) {
    double scalex, scaley;

    sl_transform_get_scale_factors(ctx, surface, &scalex, &scaley);

    scalex *= buffer_scalex;
    scaley *= buffer_scaley;

    sl_transform_direct_to_host_damage(ctx, surface, x1, y1, scalex, scaley);
    sl_transform_direct_to_host_damage(ctx, surface, x2, y2, scalex, scaley);
  } else {
    double sx = buffer_scalex * ctx->scale;
    double sy = buffer_scaley * ctx->scale;

    // Enclosing rect after scaling and outset by one pixel to account for
    // potential filtering.
    *x1 = MAX(MIN_SIZE, (*x1) - 1) / sx;
    *y1 = MAX(MIN_SIZE, (*y1) - 1) / sy;
    *x2 = ceil(MIN((*x2) + 1, MAX_SIZE) / sx);
    *y2 = ceil(MIN((*y2) + 1, MAX_SIZE) / sy);
  }
}

void sl_transform_host_to_guest(struct sl_context* ctx,
                                struct sl_host_surface* surface,
                                int32_t* x,
                                int32_t* y) {
  if (ctx->use_direct_scale) {
    sl_transform_direct_to_guest(ctx, surface, x, y);
  } else {
    if (ctx->stable_scaling) {
      stable_scale_host_to_guest(x, ctx->scale);
      stable_scale_host_to_guest(y, ctx->scale);
    } else {
      (*x) *= ctx->scale;
      (*y) *= ctx->scale;
    }
  }
}

void sl_transform_host_to_guest_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* x,
                                      wl_fixed_t* y) {
  if (ctx->use_direct_scale) {
    sl_transform_direct_to_guest_fixed(ctx, surface, x, y);
  } else {
    double dx = wl_fixed_to_double(*x);
    double dy = wl_fixed_to_double(*y);

    dx *= ctx->scale;
    dy *= ctx->scale;

    *x = wl_fixed_from_double(dx);
    *y = wl_fixed_from_double(dy);
  }
}

void sl_transform_pointer(struct sl_context* ctx,
                          struct sl_host_surface* surface,
                          wl_fixed_t* x,
                          wl_fixed_t* y) {
  sl_transform_host_to_guest_fixed(ctx, surface, x, y);
  if (surface && surface->window && surface->window->viewport_override) {
    double dx = wl_fixed_to_double(*x);
    double dy = wl_fixed_to_double(*y);
    dx *= surface->window->viewport_pointer_scale;
    dy *= surface->window->viewport_pointer_scale;
    *x = wl_fixed_from_double(dx);
    *y = wl_fixed_from_double(dy);
  }
}

void sl_transform_host_to_guest_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* coord,
                                      uint32_t axis) {
  if (ctx->use_direct_scale) {
    sl_transform_direct_to_guest_fixed(ctx, surface, coord, axis);
  } else {
    double dx = wl_fixed_to_double(*coord);
    dx *= ctx->scale;
    *coord = wl_fixed_from_double(dx);
  }
}

void sl_transform_guest_to_host(struct sl_context* ctx,
                                struct sl_host_surface* surface,
                                int32_t* x,
                                int32_t* y) {
  if (ctx->use_direct_scale) {
    sl_transform_direct_to_host(ctx, surface, x, y);
  } else {
    if (ctx->stable_scaling) {
      stable_scale_guest_to_host(x, ctx->scale);
      stable_scale_guest_to_host(y, ctx->scale);
    } else {
      (*x) /= ctx->scale;
      (*y) /= ctx->scale;
    }
  }
}

void sl_transform_guest_to_host_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* x,
                                      wl_fixed_t* y) {
  if (ctx->use_direct_scale) {
    sl_transform_direct_to_host_fixed(ctx, surface, x, y);
  } else {
    double dx = wl_fixed_to_double(*x);
    double dy = wl_fixed_to_double(*y);

    dx /= ctx->scale;
    dy /= ctx->scale;

    *x = wl_fixed_from_double(dx);
    *y = wl_fixed_from_double(dy);
  }
}

void sl_transform_guest_to_host_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* coord,
                                      uint32_t axis) {
  if (ctx->use_direct_scale) {
    sl_transform_direct_to_host_fixed(ctx, surface, coord, axis);
  } else {
    double dx = wl_fixed_to_double(*coord);
    dx /= ctx->scale;
    *coord = wl_fixed_from_double(dx);
  }
}

struct sl_host_output* sl_transform_guest_position_to_host_position(
    sl_context* ctx, sl_host_surface* surface, int32_t* x, int32_t* y) {
  sl_host_output* output = sl_infer_output_for_guest_position(ctx, *x, *y);
  assert(output);

  // Translate from global to output-relative guest coordinates
  (*x) -= output->virt_x;

  // Convert to host logical scale
  sl_transform_guest_to_host(ctx, surface, x, y);

  // Translate to global host coordinates
  (*x) += output->x;
  (*y) += output->y;

  return output;
}

struct sl_host_output* sl_transform_host_position_to_guest_position(
    sl_context* ctx, sl_host_surface* surface, int32_t* x, int32_t* y) {
  sl_host_output* output = sl_infer_output_for_host_position(ctx, *x, *y);
  assert(output);

  // Translate from global to output-local host coordinates
  (*x) -= output->x;
  (*y) -= output->y;

  // Convert to guest virtual scale
  sl_transform_host_to_guest(ctx, surface, x, y);

  // Translate to global guest coordinates
  (*x) += output->virt_x;

  return output;
}

void sl_transform_try_window_scale(struct sl_context* ctx,
                                   struct sl_host_surface* surface,
                                   int32_t width_in_pixels,
                                   int32_t height_in_pixels) {
  int32_t reverse_width = width_in_pixels;
  int32_t reverse_height = height_in_pixels;
  int32_t logical_width;
  int32_t logical_height;

  // This function should only have an effect in direct scale mode
  if (!ctx->use_direct_scale)
    return;

  // Reset scale so that calls to sl_transform_get_scale_factors will not
  // use the current scale.
  sl_transform_reset_surface_scale(ctx, surface);

  // Transform the window dimensions using the global/per-output scaling factors
  sl_transform_guest_to_host(ctx, surface, &reverse_width, &reverse_height);

  // Save the logical dimensions for later use
  logical_width = reverse_width;
  logical_height = reverse_height;

  // Transform the logical dimensions back to the virtual pixel dimensions
  sl_transform_host_to_guest(ctx, surface, &reverse_width, &reverse_height);

  // If the computed logical width or height is zero, force the
  // use of the global scaling factors

  if ((reverse_width != width_in_pixels ||
       reverse_height != height_in_pixels) &&
      (logical_width > 0 && logical_height > 0)) {
    // There is no match, let's override the scaling setting on our surface
    surface->has_own_scale = 1;
    surface->xdg_scale_x = static_cast<double>(width_in_pixels) /
                           static_cast<double>(logical_width);
    surface->xdg_scale_y = static_cast<double>(height_in_pixels) /
                           static_cast<double>(logical_height);

    surface->cached_logical_height = logical_height;
    surface->cached_logical_width = logical_width;

    // Try once more to do a full cycle (pixel -> logical -> pixel),
    // if we aren't equal, we need to force a round up on the translation
    // to the guest.

    reverse_width = width_in_pixels;
    reverse_height = height_in_pixels;

    sl_transform_guest_to_host(ctx, surface, &reverse_width, &reverse_height);
    sl_transform_host_to_guest(ctx, surface, &reverse_width, &reverse_height);

    if (reverse_width != width_in_pixels)
      surface->scale_round_on_x = true;

    if (reverse_height != height_in_pixels)
      surface->scale_round_on_y = true;
  }
}

void sl_transform_reset_surface_scale(struct sl_context* ctx,
                                      struct sl_host_surface* surface) {
  surface->has_own_scale = 0;
  surface->scale_round_on_x = surface->scale_round_on_y = false;
  surface->xdg_scale_x = surface->xdg_scale_y = 0;
}

void sl_transform_output_dimensions(struct sl_context* ctx,
                                    int32_t* width,
                                    int32_t* height) {
  if (ctx->stable_scaling) {
    stable_scale_size_host_to_guest(width, ctx->scale);
    stable_scale_size_host_to_guest(height, ctx->scale);
  } else {
    *width = (*width) * ctx->scale;
    *height = (*height) * ctx->scale;
  }
}
