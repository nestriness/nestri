// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_SCOPE_TIMER_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_SCOPE_TIMER_H_

#include <time.h>
#include <string>

// This is to time certain tasks sommelier performs at startup, see b/303549040
class ScopeTimer {
 public:
  explicit ScopeTimer(const char* event_name);
  ~ScopeTimer();

 private:
  const char* event_name_;
  timespec start_time_;
};      // class ScopeTimer
#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_SCOPE_TIMER_H_
