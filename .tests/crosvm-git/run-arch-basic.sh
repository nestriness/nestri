#/bin/bash -e

./crosvm/target/debug/crosvm run \
       	--rwdisk ./rootfs \
       	--initrd  initramfs-arch.img \
       	-p "root=/dev/vda2" \
       	./vmlinux-arch
