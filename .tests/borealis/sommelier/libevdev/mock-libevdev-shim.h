// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_LIBEVDEV_MOCK_LIBEVDEV_SHIM_H_
#define VM_TOOLS_SOMMELIER_LIBEVDEV_MOCK_LIBEVDEV_SHIM_H_

#include <gmock/gmock.h>

#include "libevdev-shim.h"  // NOLINT(build/include_directory)

class MockLibevdevShim : public LibevdevShim {
 public:
  MOCK_METHOD(struct libevdev*, new_evdev, (), (override));

  MOCK_METHOD(void, free, (struct libevdev * dev), (override));

  MOCK_METHOD(int,
              enable_event_code,
              (struct libevdev * dev,
               unsigned int type,
               unsigned int code,
               const void* data),
              (override));

  MOCK_METHOD(void,
              set_name,
              (struct libevdev * dev, const char* name),
              (override));

  MOCK_METHOD(void,
              set_id_product,
              (struct libevdev * dev, int product_id),
              (override));

  MOCK_METHOD(void,
              set_id_vendor,
              (struct libevdev * dev, int vendor_id),
              (override));

  MOCK_METHOD(void,
              set_id_bustype,
              (struct libevdev * dev, int bustype_id),
              (override));

  MOCK_METHOD(void,
              set_id_version,
              (struct libevdev * dev, int version_id),
              (override));

  MOCK_METHOD(int,
              uinput_create_from_device,
              (const struct libevdev* dev,
               int uinput_fd,
               struct libevdev_uinput** uinput_dev),
              (override));

  MOCK_METHOD(int,
              uinput_write_event,
              (const struct libevdev_uinput* uinput_dev,
               unsigned int type,
               unsigned int code,
               int value),
              (override));

  MOCK_METHOD(void,
              uinput_destroy,
              (struct libevdev_uinput * uinput_dev),
              (override));
};

#endif  // VM_TOOLS_SOMMELIER_LIBEVDEV_MOCK_LIBEVDEV_SHIM_H_
