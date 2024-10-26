// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <unistd.h>
#include <xcb/xproto.h>
#include <map>

#include "quirks/quirks.pb.h"
#include "quirks/sommelier-quirks.h"
#include "sommelier-ctx.h"      // NOLINT(build/include_directory)
#include "sommelier-logging.h"  // NOLINT(build/include_directory)
#include "sommelier-window.h"   // NOLINT(build/include_directory)
#include "xcb/xcb-shim.h"

void Quirks::Load(std::string textproto) {
  google::protobuf::TextFormat::MergeFromString(textproto, &active_config_);
  Update();
}

void Quirks::LoadFromCommaSeparatedFiles(const char* paths) {
  const char* start = paths;
  const char* end;
  do {
    // Find the next comma (or end of string).
    end = strchrnul(start, ',');
    // `start` and `end` delimit a path; load rules from it.
    std::string path(start, end - start);
    LoadFromFile(path);
    // The next string starts after the comma
    start = end + 1;
    // Terminate on null char.
  } while (*end != '\0');
}

void Quirks::LoadFromFile(std::string path) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    const char* e = strerror(errno);
    LOG(ERROR) << "failed to open quirks config: " << path << ": " << e;
    return;
  }
  google::protobuf::io::FileInputStream f(fd);
  if (google::protobuf::TextFormat::Merge(&f, &active_config_)) {
    Update();
  }
  close(fd);
}

void Quirks::PrintFeaturesEnabled(uint32_t steam_game_id) {
  // Very inefficient but straight-forward function to
  // print features enabled for the steam_game_id.
  LOG(INFO) << "Enabled features for " << steam_game_id;
  for (int i = quirks::Feature_MIN; i <= quirks::Feature_MAX; i++) {
    if (!quirks::Feature_IsValid(i) || !IsEnabled(steam_game_id, i)) {
      continue;
    }
    printf("%s\n", quirks::Feature_Name(i).c_str());
  }
}

bool Quirks::IsEnabled(uint32_t steam_game_id, int feature) {
  bool is_enabled = false;

  // Check if feature is enabled while minimizing map looks ups as much as
  // possible. |feature_to_steam_id_[feature][steam_id]| takes priority over
  // |always_features_[feature]|. This allows rule ordering to take priority
  // as we delete |feature_to_steam_id_[feature]| when
  // |always_features_[feature]| is popoulated.
  bool steam_id_override = false;
  auto steam_id_iter = feature_to_steam_id_.find(feature);
  if (steam_id_iter != feature_to_steam_id_.end()) {
    auto enabled_iter = steam_id_iter->second.find(steam_game_id);
    if (enabled_iter != steam_id_iter->second.end()) {
      steam_id_override = true;
      is_enabled = enabled_iter->second;
    }
  }
  if (!steam_id_override) {
    auto always_iter = always_features_.find(feature);
    if (always_iter != always_features_.end()) {
      is_enabled = always_iter->second;
    }
  }

  return is_enabled;
}

bool Quirks::IsEnabled(struct sl_window* window, int feature) {
  bool is_enabled = IsEnabled(window->steam_game_id, feature);

  if (is_enabled && sl_window_should_log_quirk(window, feature)) {
    LOG(INFO) << "Quirk " << quirks::Feature_Name(feature) << " applied to "
              << window
              << " due to rule `steam_game_id: " << window->steam_game_id
              << "`";

    // Also set a window property for additional debugging.
    // Create comma-separated list of quirks
    std::string all_quirks;
    for (int f : sl_window_logged_quirks(window)) {
      all_quirks.append(quirks::Feature_Name(f));
      all_quirks.append(",");
    }
    // Remove trailing comma
    if (!all_quirks.empty()) {
      all_quirks.pop_back();
    }
    xcb()->change_property(
        window->ctx->connection, XCB_PROP_MODE_REPLACE, window->id,
        window->ctx->atoms[ATOM_SOMMELIER_QUIRK_APPLIED].value, XCB_ATOM_STRING,
        8, all_quirks.size(), all_quirks.data());
  }
  return is_enabled;
}

void Quirks::Update() {
  feature_to_steam_id_.clear();
  always_features_.clear();

  for (quirks::SommelierRule rule : active_config_.sommelier()) {
    // For now, only support a single instance of a single condition.
    if (rule.condition_size() != 1) {
      continue;
    }
    if (rule.condition()[0].has_steam_game_id()) {
      uint32_t id = rule.condition()[0].steam_game_id();

      for (int feature : rule.enable()) {
        feature_to_steam_id_[feature][id] = true;
      }
      for (int feature : rule.disable()) {
        feature_to_steam_id_[feature][id] = false;
      }
    } else if (rule.condition()[0].has_always() &&
               rule.condition()[0].always()) {
      for (int feature : rule.enable()) {
        always_features_[feature] = true;
        // Clear Steam ID definitions defined so far, so that this always rule
        // takes priority since it is defined later.
        feature_to_steam_id_.erase(feature);
      }
      for (int feature : rule.disable()) {
        always_features_[feature] = false;
        // Clear Steam ID definitions defined so far, so that this always rule
        // takes priority since it is defined later.
        feature_to_steam_id_.erase(feature);
      }
    }
  }
}
