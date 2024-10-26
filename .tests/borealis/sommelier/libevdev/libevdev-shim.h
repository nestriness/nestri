// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_LIBEVDEV_LIBEVDEV_SHIM_H_
#define VM_TOOLS_SOMMELIER_LIBEVDEV_LIBEVDEV_SHIM_H_

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

class LibevdevShim {
 public:
  LibevdevShim() = default;
  LibevdevShim(LibevdevShim&&) = delete;
  LibevdevShim& operator=(LibevdevShim&&) = delete;

  virtual ~LibevdevShim() = default;

  virtual struct libevdev* new_evdev(void);
  virtual void free(struct libevdev* dev);
  virtual int enable_event_code(struct libevdev* dev,
                                unsigned int type,
                                unsigned int code,
                                const void* data);
  virtual void set_name(struct libevdev* dev, const char* name);
  virtual void set_id_product(struct libevdev* dev, int product_id);
  virtual void set_id_vendor(struct libevdev* dev, int vendor_id);
  virtual void set_id_bustype(struct libevdev* dev, int bustype);
  virtual void set_id_version(struct libevdev* dev, int version);

  virtual int uinput_create_from_device(const struct libevdev* dev,
                                        int uinput_fd,
                                        struct libevdev_uinput** uinput_dev);
  virtual int uinput_write_event(const struct libevdev_uinput* uinput_dev,
                                 unsigned int type,
                                 unsigned int code,
                                 int value);
  virtual void uinput_destroy(struct libevdev_uinput* uinput_dev);
};

class Libevdev {
 public:
  static LibevdevShim* Get();
  static void Set(LibevdevShim* shim);

 private:
  static LibevdevShim* singleton;
};

#endif  // VM_TOOLS_SOMMELIER_LIBEVDEV_LIBEVDEV_SHIM_H_
