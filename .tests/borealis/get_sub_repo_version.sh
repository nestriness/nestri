#!/bin/bash

# Function to retrieve subrepo version in r{ORDER}.{HASH} format
get_subrepo_version() {
  # Define variables
  local subrepo=$1
  local subrepo_dir

  # Construct subrepo directory path
  subrepo_dir="${subrepo//./\/}"

 echo "${subrepo_dir}"
  # Check if subrepo directory exists
  if [ ! -d "$subrepo_dir" ]; then
    echo "Error: Subrepo directory '$subrepo_dir' does not exist."
    return 1
  fi

  # Retrieve ORDER (commit count) and HASH (short HEAD hash)
  local order=$(git -C "$subrepo_dir" rev-list --count HEAD)
  local hash=$(git -C "$subrepo_dir" rev-parse --short HEAD)

  # Print version in r{ORDER}.{HASH} format
  echo "r${order}.${hash}"
}

