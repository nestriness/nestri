#!/bin/bash

# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Convert a docker container image to a borealis VM image.
# Turns a docker image into a CrOS image. In addition to pulling the filesystem
# out, we also tweak it slightly in ways that docker doesnt like.

set -e

# Default values
OUTPUT="vm_rootfs.img"
SHRINK=true
DOCKER_IMAGE="crosvm"
DEFAULT_IMG_NAME="rootfs.ext4"

function get_container_for_export {
    local docker_image_name=$1
    container=$(sudo docker create "$docker_image_name")
    echo "$container"
}

function extract_container_to_dir {
    local container_id=$1
    local output_dir=$2
    echo "Extracting to $output_dir - This may take a while"
    sudo docker export "$container_id" | tar -x -C "$output_dir"
}

function massage_sysroot {
    local sysroot=$1
    sudo chmod -R a+rx "$sysroot"
    sudo rm -f "$sysroot/etc/resolv.conf"
    sudo ln -s /run/resolv.conf "$sysroot/etc/resolv.conf"
    echo "Chromebook" | sudo tee "$sysroot/etc/hostname" > /dev/null
}

function create_fs_image {
    local sysroot=$1
    local image=$2
    local shrink=$3

    du_output=$(sudo du -bsx "$sysroot" | cut -f1)
    src_size=$((du_output))
    img_size=$((src_size * 2))

    # Remove the image if it exists
    [ -f "$image" ] && rm "$image"

    # Create a file of the appropriate size
    dd if=/dev/zero of="$image" bs=1 count=0 seek="$img_size"

    # Convert the file into an ext4 image
    sudo /sbin/mkfs.ext4 -F -m 0 -i 16384 -b 4096 -O ^has_journal "$image"

    # Copy the sysroot into the image
    mnt_dir=$(mktemp -d)
    sudo mount -t ext4 -o loop "$image" "$mnt_dir"
    mount_success=$?
    if [ $mount_success -ne 0 ]; then
        echo "Mount failed, aborting."
        rm -rf "$mnt_dir"
        exit 1
    fi

    trap "sudo umount $mnt_dir && rm -rf $mnt_dir" EXIT

    sudo rsync -aH "$sysroot/" "$mnt_dir/"
    sudo chown -R root:root "$mnt_dir/root"
    sudo chown root:root "$mnt_dir/var/empty"
    sudo chown root:root "$mnt_dir/usr/lib/dbus-1.0/dbus-daemon-launch-helper"
    sudo chmod +s "$mnt_dir/usr/lib/dbus-1.0/dbus-daemon-launch-helper"

    sudo umount "$mnt_dir"
    rm -rf "$mnt_dir"
    trap - EXIT

    # Shrink the image to save space
    if [ "$shrink" = true ]; then
        sudo /sbin/e2fsck -y -f "$image"
        sudo /sbin/resize2fs -M "$image"
    fi

    echo "Filesystem image created at: $image"
}

function convert {
    local docker_image_name=$1
    local output=$2
    local should_shrink=$3

    container_id=$(get_container_for_export "$docker_image_name")
    trap "sudo docker container rm $container_id" EXIT

    tempdir=$(mktemp -d)
    trap "rm -rf $tempdir" EXIT

    echo "Temporary directory created at: $tempdir"

    extract_container_to_dir "$container_id" "$tempdir"
    massage_sysroot "$tempdir"
    create_fs_image "$tempdir" "$output" "$should_shrink"
    sudo chmod -R u+rw "$tempdir"
}

function usage {
    echo "Usage: $0 [--output OUTPUT] [--noshrink] [--docker-image DOCKER_IMAGE]"
    echo "  --output OUTPUT          Name of the output image you will create (default: vm_rootfs.img)"
    echo "  --noshrink               Do not shrink the disk image"
    echo "  --docker-image DOCKER_IMAGE  Name of the docker image you want to export"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        --noshrink)
            SHRINK=false
            shift
            ;;
        --docker-image)
            DOCKER_IMAGE="$2"
            shift 2
            ;;
        *)
            usage
            ;;
    esac
done

echo "Starting conversion..."
convert "$DOCKER_IMAGE" "$OUTPUT" "$SHRINK"
echo "Conversion completed. Output image: $OUTPUT"
