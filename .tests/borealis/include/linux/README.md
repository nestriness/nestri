Warning
=======

The virtio headers are normally distributed as kernel patches, i.e. they are
userspace kernel extensions. Since we are using docker we inherit the kernel
userspace of whatever host you `docker build` on, which is probably not
virtio-enabled.

The files here are copies of the kernel headers, which allows you to compile.
Though we have no way of checking when the patches change, and therefore no way
of stopping the headers from diverging from the "originals".
