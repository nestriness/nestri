// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <unistd.h>

#include "quirks/quirks.pb.h"
#include "sommelier-quirks.h"  // NOLINT(build/include_directory)
#include "sommelier-window.h"  // NOLINT(build/include_directory)
#include "testing/x11-test-base.h"

namespace vm_tools::sommelier {

using QuirksTest = X11TestBase;

TEST_F(QuirksTest, LoadsFilesFromCommandLine) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;
  sl_window* game456 = CreateToplevelWindow();
  game456->steam_game_id = 456;
  char filename1[L_tmpnam];
  char filename2[L_tmpnam];
  {
    std::ofstream file1(std::tmpnam(filename1), std::ofstream::out);
    file1 << "sommelier { \n"
             "  condition { steam_game_id: 123 }\n"
             "  enable: FEATURE_X11_MOVE_WINDOWS\n"
             "}";
  }
  {
    std::ofstream file2(std::tmpnam(filename2), std::ofstream::out);
    file2 << "sommelier { \n"
             "  condition { steam_game_id: 456 }\n"
             "  enable: FEATURE_X11_MOVE_WINDOWS\n"
             "}";
  }

  std::stringstream paths;
  paths << filename1 << "," << filename2;
  ctx.quirks.LoadFromCommaSeparatedFiles(paths.str().c_str());

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
  EXPECT_TRUE(ctx.quirks.IsEnabled(game456, quirks::FEATURE_X11_MOVE_WINDOWS));
  unlink(filename1);
  unlink(filename2);
}

TEST_F(QuirksTest, ShouldSelectivelyEnableFeatures) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;
  sl_window* game456 = CreateToplevelWindow();
  game456->steam_game_id = 456;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
  EXPECT_FALSE(ctx.quirks.IsEnabled(game456, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, LaterRulesTakePriority) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  // Act: Load conflicting rules.
  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");
  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  // Assert: Feature is disabled, since that rule was last.
  EXPECT_FALSE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));

  // Act: Load another rule that enables the feature.
  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  // Assert: Feature is now enabled.
  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, EmptyConditionsAreFalse) {
  sl_window* window = CreateToplevelWindow();

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_FALSE(ctx.quirks.IsEnabled(window, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, AllConditionsMustMatch) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  condition { steam_game_id: 456 }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_FALSE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, SetsQuirkAppliedProperty) {
  xcb.DelegateToFake();
  sl_window* game123 = CreateToplevelWindow();
  xcb.create_window(nullptr, 32, game123->id, XCB_WINDOW_NONE, 0, 0, 800, 600,
                    0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0,
                    nullptr);
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");
  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));

  EXPECT_EQ(StringPropertyForTesting(
                game123->id, ctx.atoms[ATOM_SOMMELIER_QUIRK_APPLIED].value),
            "FEATURE_X11_MOVE_WINDOWS");
}

TEST_F(QuirksTest, ConditionAlwaysAppliesToAllWindows) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;
  sl_window* game456 = CreateToplevelWindow();
  game456->steam_game_id = 456;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
  EXPECT_TRUE(ctx.quirks.IsEnabled(game456, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysFalseDoesNotMatchToAnything) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;
  sl_window* game456 = CreateToplevelWindow();
  game456->steam_game_id = 456;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: false }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_FALSE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
  EXPECT_FALSE(ctx.quirks.IsEnabled(game456, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysTrueButSteamIdFalse) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_FALSE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysDisableButSteamIdEnable) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysSteamIdBothEnable) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysSteamIdBothDisable) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_FALSE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysAfterSteamId) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysSandwiched) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;
  sl_window* game456 = CreateToplevelWindow();
  game456->steam_game_id = 456;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { steam_game_id: 123 }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { steam_game_id: 456 }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
  EXPECT_FALSE(ctx.quirks.IsEnabled(game456, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysMultiple) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_BLACK_SCREEN_FIX\n"
      "}");

  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_BLACK_SCREEN_FIX));
}

TEST_F(QuirksTest, ConditionAlwaysMultipleLastDefinedHasPriority1) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { always: true }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");
  EXPECT_FALSE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

TEST_F(QuirksTest, ConditionAlwaysMultipleLastDefinedHasPriority2) {
  sl_window* game123 = CreateToplevelWindow();
  game123->steam_game_id = 123;

  ctx.quirks.Load(
      "sommelier { \n"
      "  condition { always: true }\n"
      "  disable: FEATURE_X11_MOVE_WINDOWS\n"
      "}\n"
      "sommelier { \n"
      "  condition { always: true }\n"
      "  enable: FEATURE_X11_MOVE_WINDOWS\n"
      "}");
  EXPECT_TRUE(ctx.quirks.IsEnabled(game123, quirks::FEATURE_X11_MOVE_WINDOWS));
}

}  // namespace vm_tools::sommelier
