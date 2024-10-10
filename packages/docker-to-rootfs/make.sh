#!/bin/bash

# Define colors
COL_RED="\033[0;31m"
COL_GRN="\033[0;32m"
COL_END="\033[0m"

# Get current user and group IDs
USER_ID=$(id -u)
GROUP_ID=$(id -g)

# Set default disk size
VM_DISK_SIZE_MB=${VM_DISK_SIZE_MB:-4096}

# Set repository name
REPO="nestri"

# Function to create a tar archive from a Docker image
create_tar() {
  local name=$1
  echo -e "\n${COL_GRN}[Dump $name directory structure to tar archive]${COL_END}"
  docker export -o $name.tar $(docker run -d $name /bin/true)
}

# Function to extract a tar archive
extract_tar() {
  local name=$1
  echo -e "\n${COL_GRN}[Extract $name tar archive]${COL_END}"
  docker run -it \
    -v $(pwd):/os:rw \
    $REPO/builder bash -c "mkdir -p /os/$name.dir && tar -C /os/$name.dir --numeric-owner -xf /os/$name.tar"
}

# Function to create a disk image
create_image() {
  local name=$1
  echo -e "\n${COL_GRN}[Create $name disk image]${COL_END}"
  docker run -it \
    -v $(pwd):/os:rw \
    -e DISTR=$name \
    --privileged \
    --cap-add SYS_ADMIN \
    $REPO/builder bash /os/create_image.sh $USER_ID $GROUP_ID $VM_DISK_SIZE_MB
}

# Function to ensure builder is ready
ensure_builder() {
  echo -e "\n${COL_GRN}[Ensure builder is ready]${COL_END}"
  if [ "$(docker images -q $REPO/builder)" = '' ]; then
    docker build -f Dockerfile -t $REPO/builder .
  fi
}

# Function to run builder interactively
run_builder_interactive() {
  docker run -it \
    -v $(pwd):/os:rw \
    --cap-add SYS_ADMIN \
    $REPO/builder bash
}

# Function to clean up
clean_up() {
  echo -e "\n${COL_GRN}[Remove leftovers]${COL_END}"
  rm -rf mnt debian.* alpine.* ubuntu.*
  clean_docker_procs
  clean_docker_images
}

# Function to clean Docker processes
clean_docker_procs() {
  echo -e "\n${COL_GRN}[Remove Docker Processes]${COL_END}"
  if [ "$(docker ps -qa -f=label=com.iximiuz-project=$REPO)" != '' ]; then
    docker rm $(docker ps -qa -f=label=com.iximiuz-project=$REPO)
  else
    echo "<noop>"
  fi
}

# Function to clean Docker images
clean_docker_images() {
  echo -e "\n${COL_GRN}[Remove Docker Images]${COL_END}"
  if [ "$(docker images -q $REPO/*)" != '' ]; then
    docker rmi $(docker images -q $REPO/*)
  else
    echo "<noop>"
  fi
}

# Main script
if [ $# -lt 1 ]; then
  echo "Usage: $0 <image_name> [clean|builder-interactive]"
  exit 1
fi

IMAGE_NAME=$1

case $2 in
  builder-interactive)
    run_builder_interactive
    ;;
  clean)
    clean_up
    ;;
esac

ensure_builder
create_tar $IMAGE_NAME
extract_tar $IMAGE_NAME
create_image $IMAGE_NAME

# Extract kernel `` virt-builder --get-kernel "${IMAGE_NAME}.img" -o . ``