// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-util.h"  // NOLINT(build/include_directory)

#include <stdarg.h>
#include <stdio.h>

// Performs an asprintf operation and checks the result for validity and calls
// abort() if there's a failure. Returns a newly allocated string rather than
// taking a double pointer argument like asprintf.
__attribute__((__format__(__printf__, 1, 0))) char* sl_xasprintf(
    const char* fmt, ...) {
  char* str;
  va_list args;
  va_start(args, fmt);
  int rv = vasprintf(&str, fmt, args);
  assert(rv >= 0);
  UNUSED(rv);
  va_end(args);
  return str;
}

#define DEFAULT_DELETER(TypeName, DeleteFunction)            \
  namespace std {                                            \
  void default_delete<TypeName>::operator()(TypeName* ptr) { \
    DeleteFunction(ptr);                                     \
  }                                                          \
  }

DEFAULT_DELETER(struct wl_event_source, wl_event_source_remove);
