#/bin/bash -e

./crosvm/target/debug/crosvm run --rwdisk ./rootfs --initrd ./initrd.img-6.1.0-9-amd64 -c 3 -m 4096 --net tap-name=tap0 -p "root=/dev/vda1" ./vmlinuz-6.1.0-9-amd64
