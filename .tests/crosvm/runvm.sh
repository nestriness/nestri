#/bin/bash -e

sudo ./crosvm run --cpus 4 --mem 4096 --gpu backend=virglrenderer,width=1920,height=1080 --net tap-name=crosvm_tap --rwdisk ./rootfs --initrd ./initrd-6.11.0-1-default -p "root=/dev/vda1" ./vmlinuz-6.11.0-1-default
