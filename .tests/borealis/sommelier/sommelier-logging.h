// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Very simple logger that is similar to Chromium's logger. Example usage:
//   LOG(INFO) << "hello world";
//   LOG(INFO) << window << " says hello world";

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_LOGGING_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_LOGGING_H_

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

#include "sommelier.h"         // NOLINT(build/include_directory)
#include "sommelier-window.h"  // NOLINT(build/include_directory)

// Build Sommelier with -Dlog_level=X to define LOG_LEVEL
#ifndef LOG_LEVEL
// Default log level is INFO
#define LOG_LEVEL 0
#endif  // LOG_LEVEL

// Predefined log levels
constexpr int LOG_LEVEL_VERBOSE = -1;
constexpr int LOG_LEVEL_INFO = 0;
constexpr int LOG_LEVEL_WARNING = 1;
constexpr int LOG_LEVEL_ERROR = 2;
constexpr int LOG_LEVEL_FATAL = 3;

#define LOG(level)                                                            \
  ::logging::Log(LOG_LEVEL_##level, __func__, ::logging::file_name(__FILE__), \
                 __LINE__)

namespace logging {

std::string file_name(std::string file_path);
std::string log_level_to_string(int level);

// not thread-safe
extern int64_t min_log_level;
inline void set_min_log_level(int64_t level) {
  min_log_level = level;
}

class Log {
 private:
  std::stringstream log_content;

 public:
  std::string function;
  std::string file;
  int line;
  int log_level;

  explicit Log(int log_level,
               std::string function,
               std::string file,
               int line) {
    this->log_level = log_level;
    this->function = function;
    this->file = file;
    this->line = line;
  }

  template <typename T>
  Log& operator<<(T const& value) {
    if (log_level >= min_log_level) {
      this->log_content << value;
    }
    return *this;
  }

  Log& operator<<(sl_window* window) {
    *this << "(" << window->name << "#" << std::hex << window->id << ")"
          << std::dec;
    return *this;
  }

#ifdef GAMEPAD_SUPPORT
  Log& operator<<(sl_host_gamepad* host_gamepad) {
    *this << "(name=" << host_gamepad->name << ", bus=" << host_gamepad->bus
          << ", vendor_id=" << std::hex << host_gamepad->vendor_id
          << ", product_id=" << host_gamepad->product_id
          << ", version=" << host_gamepad->version << std::dec
          << ", input_mapping="
          << (host_gamepad->input_mapping ? host_gamepad->input_mapping->id
                                          : "none")
          << ")";
    return *this;
  }
#endif

  ~Log() {
    // Example expected usage:
    //   LOG(INFO) << "hello world ";
    // Temporary objects are destroyed after the end of the expression, which
    // means this destructor is called at 'semicolon', resulting in printing to
    // cerr.
    if (this->log_content.peek() == EOF) {
      return;
    }
    std::cerr << log_level_to_string(this->log_level) << " <" << this->file
              << ":" << this->line << "> " << this->function << ": "
              << this->log_content.rdbuf() << std::endl;
  }
};

}  // namespace logging

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_LOGGING_H_
