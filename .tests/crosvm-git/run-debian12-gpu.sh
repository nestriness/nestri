#/bin/bash -e

./crosvm/target/debug/crosvm run --rwdisk ./rootfs --initrd ./initrd.img-6.1.0-9-amd64 --cpus 3 --mem 4096 --gpu backend=virglrenderer,width=1920,height=1080 --wayland-sock $XDG_RUNTIME_DIR/wayland-1 --x-display :0 -p "root=/dev/vda1" ./vmlinuz-6.1.0-9-amd64
