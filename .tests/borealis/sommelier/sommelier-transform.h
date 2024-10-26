// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_TRANSFORM_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_TRANSFORM_H_

#include "sommelier.h"      // NOLINT(build/include_directory)
#include "sommelier-ctx.h"  // NOLINT(build/include_directory)

// Direct Scaling Mode Explained:
//
// It will be helpful to define the 3 coordinate spaces that we need to
// manage:
//
// 1. Physical Coordinate Space: This refers to the actual physical dimensions
//    of the devices display. Typical sizes would be 3840x2160, 1920x1080, etc.
//
// 2. Virtual Coordinate Space: This refers to the coordinate space that is
//    formed by multiplying the scale factor with the physical dimensions.
//    (Example: scale = 1.0, physical = 3840x2160, virtual = 3840x2160)
//    (Example: scale = 0.5, physical = 3840x2160, virtual = 1920x1080)
//    The scale factor will come from the "--scale" command line parameter or
//    from the associated environment variable.
//
// 3. Host Logical Space: The dimensions of this space are defined
//    entirely by the host. The exact dimensions are retrieved through
//    the xdg_output interface. It is assumed that there is a direct, linear
//    relationship between the logical space and the physical space on the
//    host. As an example:
//     a) A 1600x900 logical space
//     b) A 3840x2160 physical space
//
//     If we place a 1600x900 dimensioned object at the origin of the logical
//     space, it should appear as a 3840x2160 object within the physical space
//     (also at the origin).
//
// The product of the desired scale factor and the physical dimensions may
// result in non-integer values. In these cases, the result
// is rounded down towards zero (truncate). This slight modification
// will require recomputation of the scale factors to maintain consistency
// between the two coordinate spaces. For this reason, the (single) scale
// factor provided as input from the user is used to generate the virtual
// coordinates. Then once those have been computed (and rounded), the scale
// factors for each axis will then be recalculated using the virtual and
// logical dimensions. Each axis is given its own scale factor because
// it is possible for only one axis to require rounding.
//
// The logical coordinates come to us from the host. This is the
// coordinate space that the host is operating in. This can change
// based on the users scale settings. In ash-chrome, this is called
// "screen space in DPs". Each output occupies a non-overlapping
// rectangle within the logical coordinate space.
//
// The physical coordinate space is no longer necessary once the virtual
// coordinate space has been formed, so no scaling factors are needed to
// convert to that space.
//
// Xwayland operates within the virtual coordinate space and the
// host is operating within its logical space. Sommelier only needs to
// facilitate translations between these two coordinate spaces.
//
// The virtual to logical scale factors are derived from the ratios between
// the virtual coordinate spaces dimensions and the logical coordinate spaces
// dimensions. An output's location in virtual space is arbitrarily chosen by
// Sommelier without regard to its location in logical space.
//
// In this mode, a buffer that is full screen sized within Xwayland (virtual)
// will also be full screen sized in the logical coordinate space. The same
// pattern holds with a quarter resolution sized image. With a scale factor
// of 1.0, it is expected that there will be no scaling done to present the
// image onto the screen.

// Coordinate transform functions
//
// In general, the transformation functions fall under one of these
// two classes:
//
// 1. Transformations which follow the basic rules:
//    A straight multiply for host->guest and straight divide for the opposite
// 2. Transformations which perform their transformations in a slightly
//    different manner.
//
// The functions immediately following this block fall under the latter
// They are separate functions so these cases can be easily identified
// throughout the rest of the code.
//
// The functions that fall under the latter case work in the
// guest->host direction and do not have variants which work in the
// opposite direction.

// This particular function will return true if setting a destination
// viewport size is necessary. It can be false if the host/guest spaces
// matches.
// This is a potential optimization as it removes one step
// from the guest->host surface_attach cycle.
bool sl_transform_viewport_scale(struct sl_context* ctx,
                                 struct sl_host_surface* surface,
                                 double contents_scale,
                                 int32_t* width,
                                 int32_t* height);

void sl_transform_damage_coord(struct sl_context* ctx,
                               const struct sl_host_surface* surface,
                               double buffer_scalex,
                               double buffer_scaley,
                               int64_t* x1,
                               int64_t* y1,
                               int64_t* x2,
                               int64_t* y2);

// Basic Transformations
// The following transformations fall under the basic type

// 1D transformation functions have an axis specifier
// to indicate along which axis the transformation is to
// take place.
//
// The axis specifier will follow the wl_pointer::axis definitions
// 0 = vertical axis (Y)
// 1 = horizontal axis (X)

void sl_transform_host_to_guest(struct sl_context* ctx,
                                struct sl_host_surface* surface,
                                int32_t* x,
                                int32_t* y);

void sl_transform_host_to_guest_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* x,
                                      wl_fixed_t* y);

void sl_transform_host_to_guest_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* coord,
                                      uint32_t axis);

// Like sl_transform_host_to_guest, but for window positions instead of sizes.
// Accounts for outputs being positioned on the host differently than they are
// in virtual space.
struct sl_host_output* sl_transform_host_position_to_guest_position(
    struct sl_context* ctx,
    struct sl_host_surface* surface,
    int32_t* x,
    int32_t* y);

// Opposite Direction
void sl_transform_guest_to_host(struct sl_context* ctx,
                                struct sl_host_surface* surface,
                                int32_t* x,
                                int32_t* y);

void sl_transform_guest_to_host_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* x,
                                      wl_fixed_t* y);

void sl_transform_guest_to_host_fixed(struct sl_context* ctx,
                                      struct sl_host_surface* surface,
                                      wl_fixed_t* coord,
                                      uint32_t axis);

// Like sl_transform_guest_to_host, but for window positions instead of sizes.
// Accounts for outputs being positioned on the host differently than they are
// in virtual space.
struct sl_host_output* sl_transform_guest_position_to_host_position(
    struct sl_context* ctx,
    struct sl_host_surface* surface,
    int32_t* x,
    int32_t* y);

// Given the desired window size in virtual pixels, this function
// will see if it can be cleanly converted to logical coordinates and back.
//
// If the desired dimensions can be met with the default scaling factors,
// no intervention will take place.
//
// If the desired dimensions CANNOT be met with the default scaling factors,
// a set of scaling factors will be chosen to match the nearest logical
// coordinates to the desired virtual pixel dimensions. These scaling factors
// will then be used for all transformations being performed on this surface.
//
// This function is a no-op when not in direct scale mode.
void sl_transform_try_window_scale(struct sl_context* ctx,
                                   struct sl_host_surface* surface,
                                   int32_t width_in_pixels,
                                   int32_t height_in_pixels);

// Removes any custom scaling factors that have been set on the surface
// by try_window_scale
void sl_transform_reset_surface_scale(struct sl_context* ctx,
                                      struct sl_host_surface* surface);

// This function performs the physical to virtual transformation
// based on the scale factor provided by the command line/env.
// This function is called in response to the physical dimensions being sent
// by the host. The virtual dimensions are calculated by this function and
// then relayed to the guest.
void sl_transform_output_dimensions(struct sl_context* ctx,
                                    int32_t* width,
                                    int32_t* height);

// Used to transform pointer coordinates from host to guest.
void sl_transform_pointer(struct sl_context* ctx,
                          struct sl_host_surface* surface,
                          wl_fixed_t* x,
                          wl_fixed_t* y);

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_TRANSFORM_H_
