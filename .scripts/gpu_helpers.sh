#!/bin/bash -e

# Various helper functions for handling available GPUs

declare -ga gpu_map
declare -gA gpu_bus_map
declare -gA gpu_card_map
declare -gA gpu_product_map
declare -gA vendor_index_map
declare -gA vendor_full_map

# Map to help get shorter vendor identifiers
declare -A vendor_keywords=(
    ["advanced micro devices"]='amd'
    ["ati"]='amd'
    ["amd"]='amd'
    ["radeon"]='amd'
    ["nvidia"]='nvidia'
    ["intel"]='intel'
)

get_gpu_info() {
  # Clear out previous data
  gpu_map=()
  gpu_bus_map=()
  gpu_card_map=()
  gpu_product_map=()
  vendor_index_map=()
  vendor_full_map=()

  local vendor=""
  local product=""
  local bus_info=""
  local vendor_full=""

  while read -r line; do
    line="${line##*( )}"

    if [[ "${line,,}" =~ "vendor:" ]]; then
      vendor=""
      vendor_full=$(echo "$line" | awk '{$1=""; print $0}' | xargs)

      # Look for short vendor keyword in line
      for keyword in "${!vendor_keywords[@]}"; do
        if [[ "${line,,}" == *"$keyword"* ]]; then
          vendor="${vendor_keywords[$keyword]}"
          break
        fi
      done

      # If no vendor keywords match, use first word
      if [[ -z "$vendor" ]]; then
        vendor=$(echo "$vendor_full" | awk '{print tolower($1)}')
      fi
    elif [[ "${line,,}" =~ "product:" ]]; then
      product=$(echo "$line" | awk '{$1=""; print $0}' | xargs)
    elif [[ "${line,,}" =~ "bus info:" ]]; then
      bus_info=$(echo "$line" | awk '{print $3}')
    fi

    if [[ -n "$vendor" && -n "$product" && -n "$bus_info" && ! "${line,,}" =~ \*"-display" ]]; then
      # We have gathered all GPU info necessary, store it

      # Check if vendor index is being tracked
      if [[ -z "${vendor_index_map[$vendor]}" ]]; then
        # Start new vendor index tracking
        vendor_index_map[$vendor]=0
      else
        # Another GPU of same vendor, increment index
        vendor_index_map[$vendor]="$((vendor_index_map[$vendor] + 1))"
      fi

      # Resolved GPU index
      local gpu_index="${vendor_index_map[$vendor]}"
      local gpu_key="$vendor:$gpu_index"

      # Get /dev/dri/cardN of GPU
      local gpu_card=$({ ls -1d /sys/bus/pci/devices/*${bus_info#pci@}/drm/*; } 2>&1 | grep card* | grep -oP '(?<=card)\d+')

      # Store info in maps
      gpu_map+=("$gpu_key")
      gpu_bus_map["$gpu_key"]="$bus_info"
      gpu_product_map["$gpu_key"]="$product"
      vendor_full_map["$gpu_key"]="$vendor_full"

      if [[ -n "$gpu_card" ]]; then
        gpu_card_map["$gpu_key"]="$gpu_card"
      fi

      # Clear values for additional GPUs
      vendor=""
      product=""
      bus_info=""
      vendor_full=""
    fi

    if [[ "${line,,}" =~ \*"-display" ]]; then
      # New GPU found before storing, clear incomplete values to prevent mixing
      vendor=""
      product=""
      bus_info=""
      vendor_full=""
    fi
  done < <(sudo lshw -c video)
}

check_and_populate_gpus() {
  if [[ "${#gpu_map[@]}" -eq 0 ]]; then
    get_gpu_info # Gather info incase info not gathered yet
    if [[ "${#gpu_map[@]}" -eq 0 ]]; then
      echo "No GPUs found on this system" >&2
      return 1
    fi
  fi
}

check_selected_gpu() {
  local selected_gpu="${1,,}"

  if [[ ! " ${gpu_map[*]} " =~ " $selected_gpu " ]]; then
    echo "No such GPU: '$selected_gpu'" >&2
    return 1
  fi

  echo "$selected_gpu"
}

list_available_gpus() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  echo "Available GPUs:" >&2
  for gpu in "${gpu_map[@]}"; do
    echo " [$gpu] \"${gpu_product_map[$gpu]}\" @[${gpu_bus_map[$gpu]}]"
  done
}

convert_bus_id_to_xorg() {
  local bus_info="$1"
  IFS=":." read -ra bus_parts <<< "${bus_info#pci@????:}" # Remove "pci@" and the following 4 characters (domain)

  # Check if bus_info has the correct format (at least 3 parts after removing domain)
  if [[ "${#bus_parts[@]}" -lt 3 ]]; then
    echo "Invalid bus info format: $bus_info" >&2
    return 1
  fi

  # Convert each part from hexadecimal to decimal
  bus_info_xorg="PCI:"
  for part in "${bus_parts[@]}"; do
    bus_info_xorg+="$((16#$part)):"
  done
  bus_info_xorg="${bus_info_xorg%:}" # Remove the trailing colon

  echo "$bus_info_xorg"
}

print_gpu_info() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  echo "[$selected_gpu]"
  echo " Vendor: ${vendor_full_map[$selected_gpu]}"
  echo " Product: ${gpu_product_map[$selected_gpu]}"
  echo " Bus: ${gpu_bus_map[$selected_gpu]}"

  # Check if card path was found
  if [[ "${gpu_card_map[$selected_gpu]}" ]]; then
    echo " Card: /dev/dri/card${gpu_card_map[$selected_gpu]}"
  fi

  echo
}

get_gpu_vendor() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  echo "${selected_gpu%%:*}"
}

get_gpu_vendor_full() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  echo "${vendor_full_map[$selected_gpu]}"
}

get_gpu_index() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  echo "${selected_gpu#*:}"
}

get_gpu_product() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  echo "${gpu_product_map[$selected_gpu]}"
}

get_gpu_bus() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  echo "${gpu_bus_map[$selected_gpu]}"
}

get_gpu_bus_xorg() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  echo $(convert_bus_id_to_xorg "${gpu_bus_map[$selected_gpu]}")
}

get_gpu_card() {
  if ! check_and_populate_gpus; then
    return 1
  fi

  local selected_gpu
  if ! selected_gpu=$(check_selected_gpu "$1"); then
    return 1
  fi

  # Check if card path was found
  if [[ -z "${gpu_card_map[$selected_gpu]}" ]]; then
    echo "No card device found for GPU: $selected_gpu" >&2
    return 1
  fi

  echo "/dev/dri/card${gpu_card_map[$selected_gpu]}"
}
