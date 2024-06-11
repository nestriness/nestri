#!/bin/bash -e

# Various helper functions for handling available GPUs

get_gpu_info() {
  declare -gA gpu_map
  declare -gA gpu_product_map
  declare -gA gpu_bus_map
  gpu_index=0
  vendor=""
  product=""
  bus_info=""

  while read -r line; do
    line="${line##*( )}"

    if [[ $line =~ vendor: ]]; then
      vendor=$(echo "$line" | awk '{print tolower($2)}')
    elif [[ $line =~ product: ]]; then
      product=$(echo "$line" | awk '{$1=""; print $0}' | xargs)
    elif [[ $line =~ bus\ info: ]]; then
      bus_info=$(echo "$line" | awk '{print $3}')
    fi

    if [[ -n "$vendor" && -n "$product" && -n "$bus_info" && ! $line =~ \*-display ]]; then
      gpu_map["$vendor:$gpu_index"]=1
      gpu_product_map["$vendor:$gpu_index"]="$product"
      gpu_bus_map["$vendor:$gpu_index"]="$bus_info"
      gpu_index=$((gpu_index+1))
      vendor=""
      product=""
      bus_info=""
    fi

    if [[ $line =~ \*-display ]]; then
      vendor=""
      product=""
      bus_info=""
    fi
  done < <(sudo lshw -c video)
}

list_available_gpus() {
  get_gpu_info  # Call to populate the global arrays

  if [[ ${#gpu_map[@]} -eq 0 ]]; then
    echo "No GPUs found on this system." >&2
    exit 1
  fi

  echo "Available GPUs are:" >&2
  for gpu in "${!gpu_map[@]}"; do
    echo "- $gpu (${gpu_product_map[$gpu]}) [${gpu_bus_map[$gpu]}]"
  done
}

convert_bus_id_to_xorg() {
  local bus_info="$1"
  IFS=":." read -ra bus_parts <<< "${bus_info#pci@????:}" # Remove "pci@" and the following 4 characters (domain)

  # Check if bus_info has the correct format (at least 3 parts after removing domain)
  if [[ ${#bus_parts[@]} -lt 3 ]]; then
      echo "Invalid bus info format: $bus_info" >&2
      exit 1
  fi

  # Convert each part from hexadecimal to decimal
  bus_info_xorg="PCI:"
  for part in "${bus_parts[@]}"; do
    bus_info_xorg+="$((16#$part)):"
  done
  bus_info_xorg="${bus_info_xorg%:}"  # Remove the trailing colon

  echo "$bus_info_xorg"
}

get_selected_gpu_info() {
  local selected_gpu=$(echo "$1" | tr '[:upper:]' '[:lower:]')

  get_gpu_info  # Call to populate the global arrays

  if [[ ! ${gpu_map[$selected_gpu]} ]]; then
    echo "Invalid GPU selection: '$selected_gpu'. Please use the format 'vendor:index' (e.g., 'intel:0' or 'nvidia:1')" >&2
    list_available_gpus  # List GPUs to help out user
    exit 1
  fi

  echo "$selected_gpu (${gpu_product_map[$selected_gpu]}) [${gpu_bus_map[$selected_gpu]}]"
}
