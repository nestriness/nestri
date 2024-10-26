// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-logging.h"      // NOLINT(build/include_directory)
#include "sommelier-scope-timer.h"  // NOLINT(build/include_directory)

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#define NSEC_PER_SEC 1000000000

static inline int64_t timespec_to_ns(timespec* t) {
  return (int64_t)t->tv_sec * NSEC_PER_SEC + t->tv_nsec;
}

ScopeTimer::ScopeTimer(const char* event_name) : event_name_(event_name) {
  clock_gettime(CLOCK_MONOTONIC, &start_time_);
}

ScopeTimer::~ScopeTimer() {
  timespec end_time;
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  int64_t end = timespec_to_ns(&end_time);
  int64_t start = timespec_to_ns(&start_time_);
  int64_t diff = end - start;
  LOG(INFO) << event_name_ << ": "
            << static_cast<float>(diff) / static_cast<float>(NSEC_PER_SEC)
            << " seconds";
}
