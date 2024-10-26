// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libevdev-shim.h"  // NOLINT(build/include_directory)

struct libevdev* LibevdevShim::new_evdev(void) {
  return libevdev_new();
}
void LibevdevShim::free(struct libevdev* dev) {
  libevdev_free(dev);
}
int LibevdevShim::enable_event_code(struct libevdev* dev,
                                    unsigned int type,
                                    unsigned int code,
                                    const void* data) {
  return libevdev_enable_event_code(dev, type, code, data);
}
void LibevdevShim::set_name(struct libevdev* dev, const char* name) {
  libevdev_set_name(dev, name);
}
void LibevdevShim::set_id_product(struct libevdev* dev, int product_id) {
  libevdev_set_id_product(dev, product_id);
}
void LibevdevShim::set_id_vendor(struct libevdev* dev, int vendor_id) {
  libevdev_set_id_vendor(dev, vendor_id);
}
void LibevdevShim::set_id_bustype(struct libevdev* dev, int bustype) {
  libevdev_set_id_bustype(dev, bustype);
}
void LibevdevShim::set_id_version(struct libevdev* dev, int version) {
  libevdev_set_id_version(dev, version);
}

int LibevdevShim::uinput_create_from_device(
    const struct libevdev* dev,
    int uinput_fd,
    struct libevdev_uinput** uinput_dev) {
  return libevdev_uinput_create_from_device(dev, uinput_fd, uinput_dev);
}
int LibevdevShim::uinput_write_event(const struct libevdev_uinput* uinput_dev,
                                     unsigned int type,
                                     unsigned int code,
                                     int value) {
  return libevdev_uinput_write_event(uinput_dev, type, code, value);
}
void LibevdevShim::uinput_destroy(struct libevdev_uinput* uinput_dev) {
  libevdev_uinput_destroy(uinput_dev);
}

LibevdevShim* Libevdev::singleton = nullptr;

LibevdevShim* Libevdev::Get() {
  return singleton;
}

void Libevdev::Set(LibevdevShim* shim) {
  singleton = shim;
}
