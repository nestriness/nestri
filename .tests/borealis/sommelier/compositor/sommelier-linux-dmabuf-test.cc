// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../sommelier.h"                  // NOLINT(build/include_directory)
#include "../sommelier-ctx.h"              // NOLINT(build/include_directory)
#include "../testing/wayland-test-base.h"  // NOLINT(build/include_directory)
#include "sommelier-linux-dmabuf.h"        // NOLINT(build/include_directory)
#include "sommelier-linux-dmabuf-test.h"   // NOLINT(build/include_directory)

#include "linux-dmabuf-unstable-v1-client-protocol.h"  // NOLINT(build/include_directory)

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <libdrm/drm_fourcc.h>
#include <memory>
#include <vector>

#include "mock-xdg-shell-shim.h"  // NOLINT(build/include_directory)
#include "xdg-shell-shim.h"       // NOLINT(build/include_directory)

namespace {

struct FormatModifier {
  uint32_t format;
  uint64_t modifier;

  bool operator==(const struct FormatModifier& other) const {
    return format == other.format && modifier == other.modifier;
  }
};

bool format_in_list(uint32_t format, const std::vector<uint32_t>& format_list) {
  for (const auto& fmt : format_list) {
    if (fmt == format)
      return true;
  }
  return false;
}

bool modifier_in_list(struct FormatModifier modifier,
                      const std::vector<struct FormatModifier>& modifier_list) {
  for (auto& mod : modifier_list) {
    if (mod == modifier)
      return true;
  }
  return false;
}

}  // namespace

namespace vm_tools::sommelier {

class FakeLinuxDmabufClient : public FakeWaylandClient {
 public:
  explicit FakeLinuxDmabufClient(struct sl_context* ctx)
      : FakeWaylandClient(ctx) {
    this->ctx = ctx;
  }

  struct zwp_linux_dmabuf_v1* BindToLinuxDmabuf(uint32_t version) {
    auto* linux_dmabuf =
        static_cast<struct zwp_linux_dmabuf_v1*>(wl_registry_bind(
            client_registry, GlobalName(ctx, &zwp_linux_dmabuf_v1_interface),
            &zwp_linux_dmabuf_v1_interface, version));
    Flush();
    return linux_dmabuf;
  }

 protected:
  struct sl_context* ctx;
};

class LinuxDmabufTest : public WaylandTestBase {
 protected:
  void SetUp() override { WaylandTestBase::SetUp(); }
  void Connect() override {
    // Setup necessary shims for compatibility with base class
    set_xdg_wm_base_shim(&mock_xdg_wm_base_shim_);

    WaylandTestBase::Connect();

    // TODO(b/234899270): remove once integration bug is fixed
    ctx.enable_linux_dmabuf = true;

    // advertise global
    wl_registry* registry = wl_display_get_registry(ctx.display);
    sl_registry_handler(&ctx, registry, next_server_id++, "zwp_linux_dmabuf_v1",
                        SL_LINUX_DMABUF_MAX_VERSION);

    client = std::make_unique<FakeLinuxDmabufClient>(&ctx);
    ctx.client = client->client;
    Pump();

    ASSERT_NE(ctx.linux_dmabuf, nullptr);
  }

  void FlushClients() { wl_display_flush_clients(ctx.host_display); }

  zwp_linux_dmabuf_v1* BindClientToLinuxDmabuf(uint32_t version) {
    // need to force event handling for client->sommelier request for binding
    // linux_dmabuf. So that is the registry::bind request
    struct zwp_linux_dmabuf_v1* linux_dmabuf =
        client->BindToLinuxDmabuf(version);

    // trigger sommelier's internal bind-to-global handler
    Pump();

    linux_dmabuf_fixture = get_linux_dmabuf_test_fixture();

    return linux_dmabuf;
  }

  void SpoofServerFormatEvents() {
    for (const auto& fmt : supported_formats) {
      HostEventHandler(linux_dmabuf_fixture.proxy)
          ->format(linux_dmabuf_fixture.user_data, linux_dmabuf_fixture.proxy,
                   fmt);
    }

    for (const auto& fmt : unsupported_formats) {
      HostEventHandler(linux_dmabuf_fixture.proxy)
          ->format(linux_dmabuf_fixture.user_data, linux_dmabuf_fixture.proxy,
                   fmt);
    }

    // signal sommelier that all bind-time events have been sent by the server
    HostEventHandler(linux_dmabuf_fixture.bind_callback_proxy)
        ->done(linux_dmabuf_fixture.user_data,
               linux_dmabuf_fixture.bind_callback_proxy, 0);
  }

  void SpoofServerModifierEvents() {
    for (const auto& mod : supported_modifiers) {
      HostEventHandler(linux_dmabuf_fixture.proxy)
          ->modifier(linux_dmabuf_fixture.user_data, linux_dmabuf_fixture.proxy,
                     mod.format, mod.modifier >> 32, mod.modifier & 0xFFFFFFFF);
    }

    for (const auto& mod : unsupported_modifiers) {
      HostEventHandler(linux_dmabuf_fixture.proxy)
          ->modifier(linux_dmabuf_fixture.user_data, linux_dmabuf_fixture.proxy,
                     mod.format, mod.modifier >> 32, mod.modifier & 0xFFFFFFFF);
    }

    // signal sommelier that all bind-time events have been sent by the server
    HostEventHandler(linux_dmabuf_fixture.bind_callback_proxy)
        ->done(linux_dmabuf_fixture.user_data,
               linux_dmabuf_fixture.bind_callback_proxy, 0);
  }

  void TearDown() override {
    client->Flush();
    WaylandTestBase::TearDown();
  }

  NiceMock<MockXdgWmBaseShim> mock_xdg_wm_base_shim_;

  struct linux_dmabuf_test_fixture linux_dmabuf_fixture;

  const std::vector<uint32_t> supported_formats = {
      DRM_FORMAT_NV12,     DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888,
      DRM_FORMAT_XBGR8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_RGB565,
  };
  const std::vector<uint32_t> unsupported_formats = {
      DRM_FORMAT_RGB332,      DRM_FORMAT_XRGB2101010, DRM_FORMAT_ARGB2101010,
      DRM_FORMAT_XBGR2101010, DRM_FORMAT_ABGR2101010,
  };
  std::vector<struct FormatModifier> supported_modifiers = {
      {DRM_FORMAT_NV12, DRM_FORMAT_MOD_LINEAR},
      {DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_INVALID},
      {DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_INVALID},
      {DRM_FORMAT_XBGR8888, DRM_FORMAT_MOD_LINEAR},
      {DRM_FORMAT_ABGR8888, DRM_FORMAT_MOD_INVALID},
      {DRM_FORMAT_RGB565, DRM_FORMAT_MOD_LINEAR},
  };
  const std::vector<struct FormatModifier> unsupported_modifiers = {
      {DRM_FORMAT_RGB332, DRM_FORMAT_MOD_LINEAR},
      {DRM_FORMAT_XRGB2101010, DRM_FORMAT_MOD_INVALID},
      {DRM_FORMAT_ARGB2101010, DRM_FORMAT_MOD_INVALID},
      {DRM_FORMAT_XBGR2101010, DRM_FORMAT_MOD_LINEAR},
      {DRM_FORMAT_ABGR2101010, DRM_FORMAT_MOD_INVALID},
  };

  std::unique_ptr<FakeLinuxDmabufClient> client;
};

struct bind_callback_data {
  std::vector<uint32_t> formats;
  std::vector<struct FormatModifier> modifiers;
};

static void format_callback(void* data,
                            struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
                            uint32_t format) {
  auto& formats = static_cast<struct bind_callback_data*>(data)->formats;

  formats.push_back(format);
}

static void modifier_callback(void* data,
                              struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
                              uint32_t format,
                              uint32_t modifier_hi,
                              uint32_t modifier_lo) {
  auto& modifiers = static_cast<struct bind_callback_data*>(data)->modifiers;

  struct FormatModifier mod = {
      format, (static_cast<uint64_t>(modifier_hi) << 32) | modifier_lo};
  modifiers.push_back(mod);
}

struct zwp_linux_dmabuf_v1_listener linux_dmabuf_listener = {
    .format = format_callback,
    .modifier = modifier_callback,
};

TEST_F(LinuxDmabufTest, BindVersion1_SendsOnlyFormats) {
  zwp_linux_dmabuf_v1* linux_dmabuf = BindClientToLinuxDmabuf(1);

  struct bind_callback_data callback_data = {};
  zwp_linux_dmabuf_v1_add_listener(linux_dmabuf, &linux_dmabuf_listener,
                                   &callback_data);

  SpoofServerFormatEvents();

  FlushClients();
  client->DispatchEvents();

  ASSERT_GT(callback_data.formats.size(), 0);
  ASSERT_EQ(callback_data.modifiers.size(), 0);
}

TEST_F(LinuxDmabufTest, BindVersion2_SendsOnlyFormats) {
  zwp_linux_dmabuf_v1* linux_dmabuf = BindClientToLinuxDmabuf(2);

  struct bind_callback_data callback_data = {};
  zwp_linux_dmabuf_v1_add_listener(linux_dmabuf, &linux_dmabuf_listener,
                                   &callback_data);

  SpoofServerFormatEvents();

  FlushClients();
  client->DispatchEvents();

  ASSERT_GT(callback_data.formats.size(), 0);
  ASSERT_EQ(callback_data.modifiers.size(), 0);
}

TEST_F(LinuxDmabufTest, BindVersion2_SendsOnlySupportedFormats) {
  zwp_linux_dmabuf_v1* linux_dmabuf = BindClientToLinuxDmabuf(2);

  struct bind_callback_data callback_data;
  zwp_linux_dmabuf_v1_add_listener(linux_dmabuf, &linux_dmabuf_listener,
                                   &callback_data);

  SpoofServerFormatEvents();

  FlushClients();
  client->DispatchEvents();

  ASSERT_EQ(callback_data.formats.size(), supported_formats.size());
  for (auto& fmt : callback_data.formats) {
    ASSERT_TRUE(format_in_list(fmt, supported_formats));
  }
}

TEST_F(LinuxDmabufTest, BindVersion3_SendsModifiers) {
  zwp_linux_dmabuf_v1* linux_dmabuf = BindClientToLinuxDmabuf(3);

  struct bind_callback_data callback_data;
  zwp_linux_dmabuf_v1_add_listener(linux_dmabuf, &linux_dmabuf_listener,
                                   &callback_data);

  SpoofServerModifierEvents();

  FlushClients();
  client->DispatchEvents();

  ASSERT_GT(callback_data.modifiers.size(), 0);
}

TEST_F(LinuxDmabufTest, BindVersion3_SendsOnlySupportedModifiers) {
  zwp_linux_dmabuf_v1* linux_dmabuf = BindClientToLinuxDmabuf(3);

  struct bind_callback_data callback_data;
  zwp_linux_dmabuf_v1_add_listener(linux_dmabuf, &linux_dmabuf_listener,
                                   &callback_data);

  SpoofServerModifierEvents();

  FlushClients();
  client->DispatchEvents();

  ASSERT_EQ(callback_data.modifiers.size(), supported_modifiers.size());
  for (auto& mod : callback_data.modifiers) {
    ASSERT_TRUE(modifier_in_list(mod, supported_modifiers));
  }
}

TEST_F(LinuxDmabufTest, BindVersion4_DoesntSendBindEvents) {
  zwp_linux_dmabuf_v1* linux_dmabuf = BindClientToLinuxDmabuf(4);

  struct bind_callback_data callback_data;
  zwp_linux_dmabuf_v1_add_listener(linux_dmabuf, &linux_dmabuf_listener,
                                   &callback_data);

  FlushClients();
  client->DispatchEvents();

  ASSERT_EQ(callback_data.formats.size(), 0);
  ASSERT_EQ(callback_data.modifiers.size(), 0);
}

}  // namespace vm_tools::sommelier
