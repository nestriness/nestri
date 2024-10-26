# Gamepad support in Sommelier

When `GAMEPAD_SUPPORT` is enabled, [Sommelier](https://crsrc.org/o/src/platform2/vm_tools/sommelier/sommelier-gaming.cc)
will create a gaming seat to listen for gamepads being attached and their associated events.

Once a gamepad is added, we use `evdev` and `uinput` to emulate an Xbox One S gamepad in the VM. We subsequently forward
gamepad events from the host through that emulated gamepad. Because not every gamepad lines up precisely with an Xbox gamepad, we sometimes
have to remap input events so that they make sense.

## Adding new gamepad mappings
This guide assumes you have access to a Chromebook in [Developer Mode](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/developer_guide.md#Enter-Developer-Mode).
1. First, you'll need the product, vendor and version ID of the gamepad you’re adding. You'll also need the input event code values that correspond
with each input on your gamepad.
   * We suggest using evtest to gather this information
     1. [Open a shell](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/developer_guide.md#getting-to-a-command-prompt-on-chromiumos) on your DUT and connect your gamepad
     2. Run `evtest` and select your device
     3. Copy the ID values from the “Input device ID” section
     4. Press/use each input on your device and note what input event code values evtest observes
3. Add your mapping to [sommelier-gaming](https://crsrc.org/o/src/platform2/vm_tools/sommelier/sommelier-gaming.cc)
   1. Create a `DeviceID` struct with the details of your gamepad
   2. Create (or reuse) a mapping for your gamepad
   3. Add the `DeviceID`/mapping pair to `kDeviceMappings`

To figure out what the mapping should be, you can follow the reference table below which demonstrates which event codes are mapped to which buttons on an Xbox gamepad (which we emulate) - we suggest making the mappings locality focused, rather than glyph-focused (i.e bottom face button should map to the Xbox “A”).

Xbox One S gamepad button-event code mapping:

| Button/axes      | Input code           |
| -----------------|----------------------|
| left joystick    | ABS_X, ABX_Y         |
| right joystick.  | ABS_RX, ABS_RY       |
| directional pad  | ABS_HAT0X, ABS_HAT0Y |
| left bumper      | BTN_TL               |
| left trigger     | ABS_Z                |
| right bumper     | BTN_TR               |
| right trigger    | ABS_RZ               |
| Y                | BTN_Y                |
| B                | BTN_B                |
| A                | BTN_A                |
| X                | BTN_X                |
| select button    | BTN_SELECT           |
| Xbox button      | BTN_MODE             |
| start button     | BTN_START            |

Notes:
* If your gamepad is missing axes or buttons, you may need to do additional work to support them
  * e.g for the DS3/PS3 gamepad we needed to add additional logic to convert BTN_DPAD_* buttons into ABS_HAT* axes and "activate" those axes manually.
* You shouldn’t need to add any additional unit tests - unless you’re adding a non-conventional gamepad (as described above).
* You’ll need to follow these steps for each connection type you want to support (i.e you’ll need to add support for USB and Bluetooth separately).
* In some cases kernel versions, driver updates, and gamepad firmware updates may change the behaviour of a gamepad. Firmware updates would typically change the version ID, however.
