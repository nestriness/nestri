// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-gaming.cc"  // NOLINT

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include "libevdev/mock-libevdev-shim.h"
#include "testing/x11-test-base.h"

namespace vm_tools {
namespace sommelier {

class GamepadTest : public X11TestBase {
 public:
  void Connect() override {
    X11TestBase::Connect();

    Libevdev::Set(&libevdevshim);
  }

 protected:
  // Normally a zcr_gamepad_v2 is generated and sent by the server. We don't
  // have an easy way to do this client side and need to create it ourselves via
  // this hack.
  struct zcr_gamepad_v2* CreateGamepadProxy(
      struct zcr_gaming_seat_v2* gaming_seat) {
    return reinterpret_cast<zcr_gamepad_v2*>(wl_proxy_create(
        reinterpret_cast<wl_proxy*>(gaming_seat), &zcr_gamepad_v2_interface));
  }

  std::vector<struct sl_host_gamepad*> GetHostGamepads(struct sl_context* ctx) {
    std::vector<struct sl_host_gamepad*> host_gamepads;
    struct sl_host_gamepad* it;
    wl_list_for_each(it, &(ctx->gamepads), link) {
      host_gamepads.push_back(it);
    }
    return host_gamepads;
  }

  void BindWLSeat(struct sl_context* ctx) {
    xwayland.get()->BindToWlSeats(ctx);
    Pump();
  }

  // Helper function that sets up a gamepad_with_info successfully.
  void SetupGamepad(struct sl_context* ctx,
                    struct zcr_gamepad_v2*& gamepad,
                    struct libevdev*& ev_dev,
                    const char* name,
                    uint32_t bus,
                    uint32_t vendor_id,
                    uint32_t product_id,
                    uint32_t version) {
    BindWLSeat(ctx);
    gamepad = CreateGamepadProxy(ctx->gaming_seat);
    ev_dev = libevdev_new();
    // Make the standard expectations for a successful gamepad_added_with_info
    // request. Note that the name, bus, vendor_id, product_id and
    // version args are referring to the host controller. At this stage, we hard
    // code what the emulated controller will appear in the VM as, which is why
    // the expectations below differ from the args from the host.
    EXPECT_CALL(libevdevshim, new_evdev()).WillOnce(Return(ev_dev));
    EXPECT_CALL(libevdevshim, set_name(ev_dev, kXboxName));
    EXPECT_CALL(libevdevshim, set_id_bustype(ev_dev, kUsbBus));
    EXPECT_CALL(libevdevshim, set_id_vendor(ev_dev, kXboxVendor));
    EXPECT_CALL(libevdevshim, set_id_product(ev_dev, kXboxProduct));
    EXPECT_CALL(libevdevshim, set_id_version(ev_dev, kXboxVersion));
    for (unsigned int i = 0; i < ARRAY_SIZE(kButtons); i++) {
      EXPECT_CALL(libevdevshim,
                  enable_event_code(ev_dev, EV_KEY, kButtons[i], nullptr));
    }

    HostEventHandler(ctx->gaming_seat)
        ->gamepad_added_with_device_info(ctx, ctx->gaming_seat, gamepad, name,
                                         bus, vendor_id, product_id, version);
  }

  testing::StrictMock<MockLibevdevShim> libevdevshim;
};

TEST_F(GamepadTest, GamingSeatCreatedOnWLSeatBind) {
  EXPECT_EQ(ctx.gaming_seat, nullptr);

  BindWLSeat(&ctx);

  EXPECT_NE(ctx.gaming_seat, nullptr);
}

TEST_F(GamepadTest, AddedDoesNothing) {
  BindWLSeat(&ctx);
  struct zcr_gamepad_v2* gamepad = CreateGamepadProxy(ctx.gaming_seat);

  HostEventHandler(ctx.gaming_seat)
      ->gamepad_added(&ctx, ctx.gaming_seat, gamepad);

  EXPECT_EQ(GetHostGamepads(&ctx).size(), 0);
}

TEST_F(GamepadTest, AddedWithInfoSetsErrorStateOnLibevdevFail) {
  BindWLSeat(&ctx);
  struct zcr_gamepad_v2* gamepad = CreateGamepadProxy(ctx.gaming_seat);

  EXPECT_CALL(libevdevshim, new_evdev()).WillOnce(Return(nullptr));

  HostEventHandler(ctx.gaming_seat)
      ->gamepad_added_with_device_info(&ctx, ctx.gaming_seat, gamepad, "Xbox",
                                       1, 2, 3, 4);

  EXPECT_EQ(GetHostGamepads(&ctx)[0]->state, kStateError);
}

TEST_F(GamepadTest, AddedWithInfoSuccess) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);

  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  EXPECT_EQ(host_gamepads[0]->state, kStatePending);
  EXPECT_EQ(host_gamepads[0]->ev_dev, ev_dev);
}

TEST_F(GamepadTest, MultipleGamepadAddedWithInfoSuccess) {
  struct zcr_gamepad_v2* gamepad1;
  struct libevdev* ev_dev1;
  struct zcr_gamepad_v2* gamepad2;
  struct libevdev* ev_dev2;
  SetupGamepad(&ctx, gamepad1, ev_dev1, "Xbox", 1, 2, 3, 4);
  SetupGamepad(&ctx, gamepad2, ev_dev2, "Xbox", 1, 2, 3, 4);

  EXPECT_EQ(GetHostGamepads(&ctx).size(), 2);
}

TEST_F(GamepadTest, ActivatedSetsErrorStateIfGamepadNotActive) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateUnknown;

  HostEventHandler(gamepad)->activated(host_gamepads[0], gamepad);

  EXPECT_EQ(host_gamepads[0]->state, kStateError);
}

TEST_F(GamepadTest, ActivatedSetsErrorStateIfLibevdevFails) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);

  EXPECT_CALL(libevdevshim,
              uinput_create_from_device(ev_dev, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                        &host_gamepads[0]->uinput_dev))
      .WillOnce(Return(1));

  HostEventHandler(gamepad)->activated(host_gamepads[0], gamepad);

  EXPECT_EQ(host_gamepads[0]->state, kStateError);
}

TEST_F(GamepadTest, ActivatedSuccess) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);

  EXPECT_CALL(libevdevshim,
              uinput_create_from_device(ev_dev, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                        &host_gamepads[0]->uinput_dev))
      .WillOnce(Return(0));

  HostEventHandler(gamepad)->activated(host_gamepads[0], gamepad);

  EXPECT_EQ(host_gamepads[0]->state, kStateActivated);
}

TEST_F(GamepadTest, VibratorAddedDoesNothing) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);

  HostEventHandler(gamepad)->vibrator_added(GetHostGamepads(&ctx)[0], gamepad,
                                            nullptr);
}

TEST_F(GamepadTest, AxisAddedSetsErrorStateIfGamepadNotActive) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateUnknown;

  HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad, 1, 2, 3, 4,
                                        5, 6);

  EXPECT_EQ(host_gamepads[0]->state, kStateError);
}

TEST_F(GamepadTest, AxisAddedSuccess) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);

  EXPECT_CALL(libevdevshim, enable_event_code(ev_dev, EV_ABS, 1, ::testing::_));

  HostEventHandler(gamepad)->axis_added(GetHostGamepads(&ctx)[0], gamepad, 1, 2,
                                        3, 4, 5, 6);
}

TEST_F(GamepadTest, FrameDoesNothingIfGamepadNotActive) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateUnknown;

  HostEventHandler(gamepad)->frame(host_gamepads[0], gamepad, 1);
}

TEST_F(GamepadTest, FrameSuccess) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateActivated;

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_SYN, SYN_REPORT, 0));

  HostEventHandler(gamepad)->frame(host_gamepads[0], gamepad, 1);
}

TEST_F(GamepadTest, ButtonDoesNothingIfGamepadNotActive) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateUnknown;

  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, 2,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_RELEASED, 0);
}

TEST_F(GamepadTest, ButtonSuccess) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateActivated;

  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_KEY, 2, 0));

  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, 2,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_RELEASED, 0);
}

TEST_F(GamepadTest, AxisDoesNothingIfGamepadNotActive) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateUnknown;

  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, 2, 250);
}

TEST_F(GamepadTest, AxisSuccess) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateActivated;

  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS, 2,
                                 wl_fixed_to_double(250)));

  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, 2, 250);
}

TEST_F(GamepadTest, RemovedSuccess) {
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;
  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox", 1, 2, 3, 4);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);
  host_gamepads[0]->state = kStateActivated;

  EXPECT_CALL(libevdevshim, free(host_gamepads[0]->ev_dev));

  HostEventHandler(gamepad)->removed(host_gamepads[0], gamepad);

  EXPECT_EQ(GetHostGamepads(&ctx).size(), 0);
}

TEST_F(GamepadTest, MappingsWorkCorrectly) {
  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared. This insures expectations are paired to the correct
  // mappings and events.
  testing::InSequence sequence;

  BindWLSeat(&ctx);
  for (auto& it : kDeviceMappings) {
    // The DualShock3 (PS3) gamepad has special-case handling and we
    // cover this in a separate test.
    if (it.second == &kDualShock3Mapping)
      continue;
    struct zcr_gamepad_v2* gamepad;
    struct libevdev* ev_dev;
    SetupGamepad(&ctx, gamepad, ev_dev, "Xbox",
                 ZCR_GAMING_SEAT_V2_BUS_TYPE_BLUETOOTH, it.first.vendor,
                 it.first.product, it.first.version);
    std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);

    EXPECT_EQ(host_gamepads.size(), 1);

    for (auto& input : it.second->mapping) {
      EXPECT_CALL(libevdevshim, enable_event_code(ev_dev, EV_ABS, input.second,
                                                  ::testing::_));

      HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad,
                                            input.first, 2, 3, 4, 5, 6);
    }

    host_gamepads[0]->state = kStateActivated;

    for (auto& input : it.second->mapping) {
      EXPECT_CALL(libevdevshim,
                  uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS,
                                     input.second, wl_fixed_to_double(250)));
      EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                                   EV_KEY, input.second, 0));

      HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, input.first,
                                      250);
      HostEventHandler(gamepad)->button(
          host_gamepads[0], gamepad, 1, input.first,
          ZCR_GAMEPAD_V2_BUTTON_STATE_RELEASED, 0);
    }

    EXPECT_EQ(host_gamepads[0]->input_mapping, it.second);
    EXPECT_CALL(libevdevshim, free(host_gamepads[0]->ev_dev));

    HostEventHandler(gamepad)->removed(host_gamepads[0], gamepad);
  }
}

// The DualShock3 gamepad is handled differently because it involves mapping
// it's DPAD buttons to axes it doesn't have. We enable those axes
// manually and special case it's DPAD buttons to output axis events, rather
// than button events.
TEST_F(GamepadTest, DualShock3MappingWorksCorrectly) {
  BindWLSeat(&ctx);
  struct zcr_gamepad_v2* gamepad;
  struct libevdev* ev_dev;

  // The DualShock3 gamepad doesn't have these axes so we manually enable them
  // in Sommelier.
  EXPECT_CALL(libevdevshim,
              enable_event_code(::testing::_, EV_ABS, ABS_HAT0Y, ::testing::_));
  EXPECT_CALL(libevdevshim,
              enable_event_code(::testing::_, EV_ABS, ABS_HAT0X, ::testing::_));

  SetupGamepad(&ctx, gamepad, ev_dev, "Xbox",
               ZCR_GAMING_SEAT_V2_BUS_TYPE_BLUETOOTH, kDualShock3USB.vendor,
               kDualShock3USB.product, kDualShock3USB.version);
  std::vector<struct sl_host_gamepad*> host_gamepads = GetHostGamepads(&ctx);

  EXPECT_EQ(host_gamepads.size(), 1);
  EXPECT_EQ(host_gamepads[0]->input_mapping, &kDualShock3Mapping);

  // This object forces all EXPECT_CALLs to occur in the order they are
  // declared. This insures expectations are paired to the correct
  // mappings and events.
  testing::InSequence sequence;

  // Add axes.
  EXPECT_CALL(libevdevshim,
              enable_event_code(ev_dev, EV_ABS, ABS_X, ::testing::_));
  HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad, ABS_X, 2, 3,
                                        4, 5, 6);

  EXPECT_CALL(libevdevshim,
              enable_event_code(ev_dev, EV_ABS, ABS_Y, ::testing::_));
  HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad, ABS_Y, 2, 3,
                                        4, 5, 6);

  EXPECT_CALL(libevdevshim,
              enable_event_code(ev_dev, EV_ABS, ABS_RX, ::testing::_));
  HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad, ABS_RX, 2, 3,
                                        4, 5, 6);

  EXPECT_CALL(libevdevshim,
              enable_event_code(ev_dev, EV_ABS, ABS_RY, ::testing::_));
  HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad, ABS_RY, 2, 3,
                                        4, 5, 6);

  EXPECT_CALL(libevdevshim,
              enable_event_code(ev_dev, EV_ABS, ABS_Z, ::testing::_));
  HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad, ABS_Z, 2, 3,
                                        4, 5, 6);

  EXPECT_CALL(libevdevshim,
              enable_event_code(ev_dev, EV_ABS, ABS_RZ, ::testing::_));
  HostEventHandler(gamepad)->axis_added(host_gamepads[0], gamepad, ABS_RZ, 2, 3,
                                        4, 5, 6);

  host_gamepads[0]->state = kStateActivated;

  // Handle axis events.
  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS, ABS_X,
                                 wl_fixed_to_double(250)));
  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, ABS_X, 250);

  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS, ABS_Y,
                                 wl_fixed_to_double(250)));
  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, ABS_Y, 250);

  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS, ABS_RX,
                                 wl_fixed_to_double(250)));
  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, ABS_RX, 250);

  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS, ABS_RY,
                                 wl_fixed_to_double(250)));
  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, ABS_RY, 250);

  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS, ABS_Z,
                                 wl_fixed_to_double(250)));
  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, ABS_Z, 250);

  EXPECT_CALL(libevdevshim,
              uinput_write_event(host_gamepads[0]->uinput_dev, EV_ABS, ABS_RZ,
                                 wl_fixed_to_double(250)));
  HostEventHandler(gamepad)->axis(host_gamepads[0], gamepad, 1, ABS_RZ, 250);

  // Handle buttons
  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_THUMBL, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_THUMBL,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_THUMBR, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_THUMBR,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_A, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_A,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_B, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_B,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_X, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_Y,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_Y, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_X,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_TL, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_TL,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_TR, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_TR,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_SELECT, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_SELECT,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_START, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_START,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_KEY, BTN_MODE, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_MODE,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  // These buttons involve converting a button event into an axis event.
  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_ABS, ABS_HAT0X, -1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_DPAD_LEFT,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);
  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_ABS, ABS_HAT0X, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1,
                                    BTN_DPAD_RIGHT,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);
  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_ABS, ABS_HAT0Y, -1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_DPAD_UP,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);
  EXPECT_CALL(libevdevshim, uinput_write_event(host_gamepads[0]->uinput_dev,
                                               EV_ABS, ABS_HAT0Y, 1));
  HostEventHandler(gamepad)->button(host_gamepads[0], gamepad, 1, BTN_DPAD_DOWN,
                                    ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED, 0);

  EXPECT_CALL(libevdevshim, free(host_gamepads[0]->ev_dev));

  HostEventHandler(gamepad)->removed(host_gamepads[0], gamepad);
}

}  // namespace sommelier
}  // namespace vm_tools
