#/bin/bash -e

export USERVM=nestrivm

builder_args=(
  --install sudo,netplan.io,openvswitch-switch
  --run-command "useradd -m -g sudo -p '' $USERVM; chage -d 0 $USERVM"
  --hostname crosvm-test
  --copy-in "$(pwd)/guest/01-netcfg.yaml:/etc/netplan/"
  --install xfce4,git,openssh-server,mesa-utils,htop,vulkan-tools,xwayland
  -o ./rootfs
)

ID_RSA_PUB="$HOME/.ssh/id_rsa.pub"
if [ -r "${ID_RSA_PUB}" ]; then
  builder_args+=("--ssh-inject" "${USERVM}:file:${ID_RSA_PUB}")
fi

virt-builder debian-12 "${builder_args[@]}"
