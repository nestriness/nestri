// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/x11-test-base.h"

namespace vm_tools {
namespace sommelier {

using X11Test = X11TestBase;

TEST_F(X11Test, OutputScaleAndTransformAreApplied) {
  // Arrange/Act: Advertise a fake output with no scaling.
  AdvertiseOutputs(xwayland.get(),
                   {{.width_pixels = 1920, .height_pixels = 1080}});
  struct sl_host_output* output = nullptr;

  // Assert: Dimensions are unchanged.
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->virt_rotated_width, 1920);
  EXPECT_EQ(output->virt_rotated_height, 1080);

  // Act: Change the user defined zoom factor to be 150%.
  ConfigureOutput(output, {.output_scale = 1500});
  // Assert: Dimensions scaled up.
  EXPECT_EQ(output->virt_rotated_width, 1280);
  EXPECT_EQ(output->virt_rotated_height, 720);

  // Act: Change zoom factor to be 80% and rotate.
  ConfigureOutput(output,
                  {.transform = WL_OUTPUT_TRANSFORM_90, .output_scale = 800});
  // Assert: Dimensions are scaled down and rotated dimensions are set.
  EXPECT_EQ(output->virt_rotated_width, 1350);
  EXPECT_EQ(output->virt_rotated_height, 2400);
}

TEST_F(X11DirectScaleTest,
       AddingMultipleOutputsPositionsCorrectlyInDirectScale) {
  // Act: Advertise two outputs together.
  std::vector<OutputConfig> configs = {
      {.x = 0, .y = 0, .width_pixels = 1920, .height_pixels = 1080},
      {.x = 1920,
       .y = 0,
       .width_pixels = 1920,
       .height_pixels = 1080,
       .output_scale = 1250},
  };
  AdvertiseOutputs(xwayland.get(), configs);

  struct sl_host_output* output = nullptr;

  // Assert: Outputs are positioned correctly in logical space.
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, 0);
  EXPECT_EQ(output->virt_x, 0);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->x, 1920);
  EXPECT_EQ(output->virt_x, 1920);

  // Advertise another output to the left.
  AdvertiseOutputs(xwayland.get(), {{.x = -800,
                                     .y = 1080,
                                     .width_pixels = 1920,
                                     .height_pixels = 1080,
                                     .transform = WL_OUTPUT_TRANSFORM_90}});

  // Assert: Outputs are positioned correctly in logical space.
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, -800);
  EXPECT_EQ(output->virt_x, 0);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->x, 0);
  EXPECT_EQ(output->virt_x, 1080);

  output = ctx.host_outputs[2];
  EXPECT_EQ(output->x, 1920);
  EXPECT_EQ(output->virt_x, 3000);
}

TEST_F(X11Test, AddingMultipleOutputsPositionsCorrectly) {
  // Act: Advertise outputs.
  std::vector<OutputConfig> configs = {
      {.x = 0, .y = 0, .width_pixels = 1920, .height_pixels = 1080},
      {.x = 1920,
       .y = 0,
       .width_pixels = 1920,
       .height_pixels = 1080,
       .output_scale = 1250},
  };
  AdvertiseOutputs(xwayland.get(), configs);

  struct sl_host_output* output = nullptr;

  // Assert: Outputs are positioned correctly in logical space.
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, 0);
  EXPECT_EQ(output->virt_x, 0);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->x, 1920);
  EXPECT_EQ(output->virt_x, 1920);

  // Act
  AdvertiseOutputs(xwayland.get(), {{
                                       .x = -800,
                                       .y = 1080,
                                       .width_pixels = 1920,
                                       .height_pixels = 1080,
                                       .transform = WL_OUTPUT_TRANSFORM_90,
                                       .output_scale = 750,
                                   }});

  // Assert
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, -800);
  EXPECT_EQ(output->virt_x, 0);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->x, 0);
  EXPECT_EQ(output->virt_x, 1440);

  output = ctx.host_outputs[2];
  EXPECT_EQ(output->x, 1920);
  EXPECT_EQ(output->virt_x, 3360);
}

TEST_F(X11Test, OutputsPositionedCorrectlyAfterRemovingLeftOutput) {
  // Arrange: Add 3 outputs.
  std::vector<OutputConfig> configs = {
      {.x = 0, .y = 0, .width_pixels = 1920, .height_pixels = 1080},
      {.x = 1920,
       .y = 0,
       .width_pixels = 1920,
       .height_pixels = 1080,
       .transform = WL_OUTPUT_TRANSFORM_90,
       .output_scale = 2000},
      {.x = 2000, .y = 1080, .width_pixels = 1920, .height_pixels = 1080},
  };
  AdvertiseOutputs(xwayland.get(), configs);

  // Act: remove the leftmost output.
  struct sl_host_output* output = nullptr;
  output = ctx.host_outputs[0];
  RemoveOutput(output);

  // Assert: Output is removed and others are shifted left.
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, 1920);
  EXPECT_EQ(output->virt_x, 0);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->x, 2000);
  EXPECT_EQ(output->virt_x, 540);

  // outputs has length 2.
  EXPECT_EQ(ctx.host_outputs.size(), 2u);
}

TEST_F(X11Test, OutputsPositionedCorrectlyAfterRemovingMiddleOutput) {
  // Arrange: Add 3 outputs.
  std::vector<OutputConfig> configs = {
      {.x = 0, .y = 0, .width_pixels = 1920, .height_pixels = 1080},
      {.x = 1920,
       .y = 0,
       .width_pixels = 1920,
       .height_pixels = 1080,
       .transform = WL_OUTPUT_TRANSFORM_90,
       .output_scale = 2000},
      {.x = 2000, .y = 1080, .width_pixels = 1920, .height_pixels = 1080},
  };
  AdvertiseOutputs(xwayland.get(), configs);

  // Act: Remove the middle output;
  struct sl_host_output* output = nullptr;
  output = ctx.host_outputs[1];
  RemoveOutput(output);

  // Assert
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, 0);
  EXPECT_EQ(output->virt_x, 0);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->x, 2000);
  EXPECT_EQ(output->virt_x, 1920);

  // outputs has length 2.
  EXPECT_EQ(ctx.host_outputs.size(), 2u);
}

TEST_F(X11Test, OtherOutputUnchangedAfterRemovingRightOutput) {
  // Arrange: Add 2 outputs.
  std::vector<OutputConfig> configs = {
      {.x = 0, .y = 0, .width_pixels = 1920, .height_pixels = 1080},
      {.x = 1920, .y = 0}};
  AdvertiseOutputs(xwayland.get(), configs);

  // Act: Remove the rightmost output.
  struct sl_host_output* output = nullptr;
  output = ctx.host_outputs[1];
  RemoveOutput(output);

  // Assert
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, 0);
  EXPECT_EQ(output->virt_x, 0);
  // outputs has length 1.
  EXPECT_EQ(ctx.host_outputs.size(), 1u);
}

TEST_F(X11Test, RotatingOutputsShiftsNeighbouringOutputs) {
  // Arrange
  std::vector<OutputConfig> configs = {
      {.x = 0, .y = 0, .width_pixels = 1920, .height_pixels = 1080},
      {.x = 1920},
  };
  AdvertiseOutputs(xwayland.get(), configs);
  struct sl_host_output* output = nullptr;

  output = ctx.host_outputs[0];
  // Act: Rotate output by 270 degrees.
  ConfigureOutput(output, {.transform = WL_OUTPUT_TRANSFORM_270});

  // Assert: Output is rotated and next output is shifted left.
  EXPECT_EQ(output->virt_x, 0);
  EXPECT_EQ(output->virt_rotated_width, 1080);
  EXPECT_EQ(output->virt_rotated_height, 1920);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->virt_x, 1080);
}

TEST_F(X11Test, MovingOutputsShiftsOutputs) {
  // Arrange
  std::vector<OutputConfig> configs = {
      {.x = 0, .y = 0, .width_pixels = 1920, .height_pixels = 1080},
      {.x = 1920, .width_pixels = 1920, .height_pixels = 1080},
  };
  AdvertiseOutputs(xwayland.get(), configs);
  struct sl_host_output* output = nullptr;

  output = ctx.host_outputs[1];
  // Act: Move output on the right to be on the left of other output.
  ConfigureOutput(output, {.x = -1920, .y = 700});

  // Assert
  output = ctx.host_outputs[0];
  EXPECT_EQ(output->x, -1920);
  EXPECT_EQ(output->virt_x, 0);

  output = ctx.host_outputs[1];
  EXPECT_EQ(output->x, 0);
  EXPECT_EQ(output->virt_x, 1920);
}

}  // namespace sommelier
}  // namespace vm_tools
