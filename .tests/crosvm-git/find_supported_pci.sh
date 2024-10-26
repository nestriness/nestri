#!/bin/bash

# Set the path to the PCI devices directory
PCI_DEVICES_PATH="/sys/bus/pci/devices"

# Check if the path exists
if [ ! -d "$PCI_DEVICES_PATH" ]; then
  echo "Error: $PCI_DEVICES_PATH does not exist"
  exit 1
fi


# Loop through each device in the PCI devices directory
for device in "$PCI_DEVICES_PATH"/*; do
  # Check if the device is a directory (i.e., a PCI device)
  if [ -d "$device" ]; then
    # Get the device class
    device_class=$(cat "$device/class" 2>/dev/null)
    # Check if the device class is VGA or 3D
    if [ "$device_class" == "0x030000" ] || [ "$device_class" == "0x030200" ]; then
      # Get the device vendor ID
      vendor_id=$(cat "$device/vendor" 2>/dev/null)

      # Check if the vendor ID is NVIDIA (0x10de)
      if [ "$vendor_id" == "0x10de" ]; then
        echo "Error: NVIDIA PCI devices are not supported yet"
        exit 1
      else
        echo "Found supported PCI device: $device"
      fi
    fi
  fi
done
