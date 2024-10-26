#/bin/bash -e

export WAYLAND_DISPLAY=wayland-1

WAYLAND_DISPLAY=wayland-1 ./crosvm/target/debug/crosvm run --disable-sandbox --shared-dir "/run/user/1000/:wltag:type=fs" --net tap-name=tap0 --rwdisk ./rootfs --initrd ./initrd.img-6.1.0-9-amd64 --cpus 3 --mem 4096 --gpu=vulkan=true,context-types=cross-domain,backend=virglrenderer,width=1280,height=720 --wayland-sock $XDG_RUNTIME_DIR/wayland-1 --gpu-render-server path=/home/testing/Gits/virglrenderer/build/server/virgl_render_server -p "root=/dev/vda1" ./vmlinuz-6.1.0-9-amd64
