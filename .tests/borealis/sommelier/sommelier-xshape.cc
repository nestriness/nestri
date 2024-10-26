// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <pixman.h>

#include "compositor/sommelier-formats.h"  // NOLINT(build/include_directory)
#include "sommelier.h"                     // NOLINT(build/include_directory)
#include "sommelier-tracing.h"             // NOLINT(build/include_directory)
#include "sommelier-xshape.h"              // NOLINT(build/include_directory)

static void sl_clear_shape_region(sl_window* window) {
  window->shaped = false;
  pixman_region32_clear(&window->shape_rectangles);
}

static void sl_attach_shape_region(struct sl_context* ctx,
                                   xcb_window_t window) {
  sl_window* sl_window = nullptr;
  xcb_shape_get_rectangles_reply_t* reply;
  int i;

  sl_window = sl_lookup_window(ctx, window);
  if (!sl_window)
    return;

  reply = xcb_shape_get_rectangles_reply(
      ctx->connection,
      xcb_shape_get_rectangles(ctx->connection, window, XCB_SHAPE_SK_BOUNDING),
      nullptr);

  if (!reply)
    return;

  int nrects = xcb_shape_get_rectangles_rectangles_length(reply);
  xcb_rectangle_t* rects = xcb_shape_get_rectangles_rectangles(reply);

  if (!rects || nrects <= 0)
    return;

  pixman_box32_t* boxes =
      static_cast<pixman_box32_t*>(calloc(sizeof(pixman_box32_t), nrects));

  if (!boxes) {
    free(reply);
    return;
  }

  for (i = 0; i < nrects; i++) {
    boxes[i].x1 = rects[i].x;
    boxes[i].y1 = rects[i].y;

    boxes[i].x2 = rects[i].x + rects[i].width;
    boxes[i].y2 = rects[i].y + rects[i].height;
  }

  pixman_region32_init_rects(&sl_window->shape_rectangles, boxes, nrects);

  free(boxes);
  free(reply);
  sl_window->shaped = true;
}

void sl_handle_shape_notify(struct sl_context* ctx,
                            struct xcb_shape_notify_event_t* event) {
  sl_window* window = nullptr;

  window = sl_lookup_window(ctx, event->affected_window);

  if (!window)
    return;

  sl_clear_shape_region(window);

  if (event->shaped)
    sl_attach_shape_region(ctx, event->affected_window);

  return;
}

void sl_shape_query(struct sl_context* ctx, xcb_window_t xwindow) {
  xcb_shape_query_extents_reply_t* reply;
  sl_window* sl_window = nullptr;

  sl_window = sl_lookup_window(ctx, xwindow);
  if (!sl_window)
    return;

  reply = xcb_shape_query_extents_reply(
      ctx->connection, xcb_shape_query_extents(ctx->connection, xwindow),
      nullptr);

  if (!reply)
    return;

  sl_clear_shape_region(sl_window);

  if (reply->bounding_shaped) {
    sl_attach_shape_region(ctx, xwindow);
  }
}

pixman_format_code_t sl_pixman_format_for_shm_format(uint32_t shm_format) {
  assert(sl_shm_format_is_supported(shm_format));
  pixman_format_code_t fmt = PIXMAN_a1;

  switch (shm_format) {
    case WL_SHM_FORMAT_ARGB8888:
      fmt = PIXMAN_a8r8g8b8;
      break;

    case WL_SHM_FORMAT_XRGB8888:
      fmt = PIXMAN_x8r8g8b8;
      break;

    case WL_SHM_FORMAT_ABGR8888:
      fmt = PIXMAN_a8b8g8r8;
      break;

    case WL_SHM_FORMAT_XBGR8888:
      fmt = PIXMAN_x8b8g8r8;
      break;

    case WL_SHM_FORMAT_RGB565:
      fmt = PIXMAN_r5g6b5;
      break;

    default:
      assert(0);
      break;
  }

  return fmt;
}

void sl_xshape_generate_argb_image(struct sl_context* ctx,
                                   pixman_region32_t* shape,
                                   struct sl_mmap* src_mmap,
                                   pixman_image_t* dst_image,
                                   uint32_t src_shm_format) {
  int buf_width, buf_height, nrects;
  pixman_region32_t intersect_rects;
  pixman_image_t* src;

  assert(ctx);
  assert(shape);
  assert(src_mmap);
  assert(dst_image);

  buf_width = pixman_image_get_width(dst_image);
  buf_height = pixman_image_get_height(dst_image);

  if (buf_width <= 0 || buf_height <= 0)
    return;

  // Intersect with the pixmap bounds to ensure we do not perform
  // any OOB accesses
  // In addition, we can assume the dimensions of the dst_image is
  // the same size as the input image

  pixman_region32_init(&intersect_rects);
  pixman_region32_intersect_rect(&intersect_rects, shape, 0, 0, buf_width,
                                 buf_height);

  // With the blank destination image, we will take the source image and the
  // shape rectangles and generate the "stamped out" ARGB image.
  //
  // This is accomplished by clearing out the destination image to be
  // completely transparent as a first step. Then for each rectangular
  // region within the shape data, we will use pixman_image_composite to
  // copy that portion of the image from the source to the ARGB stamp out
  // buffer.
  //
  // pixman_image_composite is used as it will automatically perform pixel
  // format conversion for us.

  src = pixman_image_create_bits_no_clear(
      sl_pixman_format_for_shm_format(src_shm_format), buf_width, buf_height,
      reinterpret_cast<uint32_t*>(src_mmap->addr), src_mmap->stride[0]);

  pixman_box32_t* rects = pixman_region32_rectangles(&intersect_rects, &nrects);

  pixman_color_t clear = {.red = 0, .green = 0, .blue = 0, .alpha = 0};
  pixman_box32_t dstbox = {.x1 = 0, .y1 = 0, .x2 = buf_width, .y2 = buf_height};

  pixman_image_fill_boxes(PIXMAN_OP_SRC, dst_image, &clear, 1, &dstbox);

  for (int i = 0; i < nrects; i++) {
    pixman_image_composite(PIXMAN_OP_SRC, src, nullptr, dst_image, rects[i].x1,
                           rects[i].y1, 0, 0, rects[i].x1, rects[i].y1,
                           (rects[i].x2 - rects[i].x1),
                           (rects[i].y2 - rects[i].y1));
  }

  // Release the memory associated with the above
  pixman_image_unref(src);
  pixman_region32_fini(&intersect_rects);
}
