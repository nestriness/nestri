// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-logging.h"  // NOLINT(build/include_directory)

namespace logging {

int64_t min_log_level = LOG_LEVEL;

std::string file_name(std::string file_path) {
  // Extract file name from file path.
  auto found = file_path.find_last_of('/');
  if (found != std::string::npos) {
    return file_path.substr(found + 1);
  }
  return file_path;
}

std::string log_level_to_string(int level) {
  if (level == LOG_LEVEL_VERBOSE) {
    return "VERBOSE";
  } else if (level == LOG_LEVEL_INFO) {
    return "INFO";
  } else if (level == LOG_LEVEL_WARNING) {
    return "WARNING";
  } else if (level == LOG_LEVEL_ERROR) {
    return "ERROR";
  } else if (level == LOG_LEVEL_FATAL) {
    return "FATAL";
  }
  return std::to_string(level);
}

}  // namespace logging
