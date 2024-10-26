#/bin/bash -e

./crosvm/target/debug/crosvm run --disable-sandbox --gpu=vulkan=true,context-types=cross-domain,width=1280,height=720,backend=virglrenderer --wayland-sock $XDG_RUNTIME_DIR/wayland-1 --gpu-render-server path=/home/testing/Gits/virglrenderer/build/server/virgl_render_server --net tap-name=tap0 --rwdisk ./rootfs --initrd ./initrd.img-6.1.0-9-amd64 --cpus 3 --mem 4096 -p "root=/dev/vda1" ./vmlinuz-6.1.0-9-amd64
