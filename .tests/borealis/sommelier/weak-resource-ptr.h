// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_WEAK_RESOURCE_PTR_H_
#define VM_TOOLS_SOMMELIER_WEAK_RESOURCE_PTR_H_

#include "sommelier.h"  // NOLINT(build/include_directory)

#include <algorithm>
#include <functional>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// WeakResourcePtr is a weak pointer for a proxy object (sl_host_foo). It can
// be useful for objects with events like enter/leave, to keep track of an
// object provided in the enter event and ensure that it is not used if the
// client destroys it.
template <typename SlHostType>
class WeakResourcePtr {
 public:
  WeakResourcePtr() {
    wl_list_init(&destroy_listener_.link);
    destroy_listener_.notify = ResourceDestroyed;
  }

  ~WeakResourcePtr() { wl_list_remove(&destroy_listener_.link); }

  WeakResourcePtr& operator=(SlHostType* host) {
    if (host == host_)
      return *this;

    Reset();
    if (host) {
      host_ = host;
      wl_resource_add_destroy_listener(host_->resource, &destroy_listener_);
    }
    return *this;
  }

  operator bool() const { return host_; }
  SlHostType* operator->() const { return host_; }
  SlHostType* get() const { return host_; }

  void Reset() {
    host_ = nullptr;
    // Remove the listener
    wl_list_remove(&destroy_listener_.link);
    wl_list_init(&destroy_listener_.link);
  }

 private:
  SlHostType* host_ = nullptr;
  // This is always in an initialized state
  wl_listener destroy_listener_;

  static void ResourceDestroyed(wl_listener* listener, void* data) {
    WeakResourcePtr* ptr;
    ptr = wl_container_of(listener, ptr, destroy_listener_);
    ptr->Reset();
  }
};

#endif  // VM_TOOLS_SOMMELIER_WEAK_RESOURCE_PTR_H_
