#/bin/bash -e

virt-builder opensuse-tumbleweed --install sudo --hostname nestrivm-test --install htop -o ./rootfs
