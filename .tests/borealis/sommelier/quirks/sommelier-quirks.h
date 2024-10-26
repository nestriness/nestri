// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_QUIRKS_SOMMELIER_QUIRKS_H_
#define VM_TOOLS_SOMMELIER_QUIRKS_SOMMELIER_QUIRKS_H_

#include <map>
#include <string>
#include <utility>

#include <google/protobuf/message.h>
#include "quirks/quirks.pb.h"

class Quirks {
 public:
  // Parse `textproto` as a Config proto, and merge it into the active config.
  void Load(std::string textproto);

  // Call `LoadFromFile` for each filename separated by commas in `paths`.
  void LoadFromCommaSeparatedFiles(const char* paths);

  // Load a Config textproto from `path`, and merge it into the active config.
  void LoadFromFile(std::string path);

  // Whether the given Feature (from quirks.proto) is enabled for the given
  // `window`, according to the active config.
  bool IsEnabled(struct sl_window* window, int feature);

  // Whether the given Feature (from quirks.proto) is enabled for the given
  // game, according to the active config.
  bool IsEnabled(uint32_t steam_game_id, int feature);

  // Print all the features enabled for the game.
  void PrintFeaturesEnabled(uint32_t steam_game_id);

 private:
  // Repopulate `enabled_features_` from the rules in `active_config_`.
  void Update();

  // The active rules in protobuf form, accumulated from calls to `Load()`.
  quirks::Config active_config_;

  // The active config in a more easily queryable form.
  // This is a map of Feature ID (see quirks.proto) to Steam game ID to
  // enabled boolean. The boolean represents if the Feature is enabled for
  // the windows with that STEAM_GAME property - note that false explicitly
  // disables the property.
  std::map<int, std::map<uint32_t, bool>> feature_to_steam_id_;

  std::map<int, bool> always_features_;
};

#endif  // VM_TOOLS_SOMMELIER_QUIRKS_SOMMELIER_QUIRKS_H_
