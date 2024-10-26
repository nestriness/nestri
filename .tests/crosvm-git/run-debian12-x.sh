#/bin/bash -e

./crosvm/target/debug/crosvm run \
	--disable-sandbox \
	--cpus 4 \
	--mem 4096 \
	--gpu backend=virglrenderer,width=1920,height=1080 \
	--display-window-keyboard \
	--display-window-mouse \
	--net tap-name=crosvm_tap \
	--rwdisk ./rootfs \
	--initrd ./initrd.img-6.1.0-9-amd64 \
	-p "root=/dev/vda1" \
	./vmlinuz-6.1.0-9-amd64
