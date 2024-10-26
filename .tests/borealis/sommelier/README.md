# Sommelier - Nested Wayland compositor with support for X11 forwarding

Sommelier is an implementation of a Wayland compositor that delegates
compositing to a 'host' compositor. Sommelier includes a set of features that
allows it to run inside a tight jail or virtual machine.

Sommelier can run as service or as a wrapper around the execution of a
program. As a service, called the parent sommelier, it spawns new processes as
needed to service clients.

## Sommeliers

### Parent Sommelier

The parent sommelier instance will create a wayland socket in XDG_RUNTIME_DIR
and accept connections from regular wayland clients. Each connection will be
serviced by spawning a child sommelier process.

### X11 Sommelier

An X11 sommelier instance provides X11 forwarding. Xwayland is used to
accomplish this. A single X11 sommelier instance is typically shared across
all X11 clients as they often expect that they can use a shared X server for
communication. If the X11 sommelier instance crashes in this setup, it takes
all running X11 programs down with it. Multiple X11 sommelier instances
can be used for improved isolation or when per-client configuration is
needed, but it will be at the cost of losing the ability for programs to use
the X server for communication between each other.

### Peer Sommelier

Each Linux program that support the Wayland protocol can have its own sommelier.
This provides better use of multiple cores when servicing clients, and it
prevents errors in one client from causing other clients to crash.

## Host Compositor Channel

Sommelier needs a channel to the host compositor in order to serve Wayland
clients inside a container. If the container environment provides a socket
that can be used to establish a connection to the host compositor, then
pointing sommelier to this socket using the `--display=DISPLAY` flag is
sufficient.

### VirtWL

The VirtWL device can be used to establish a new connection when no socket
is available (typically when running inside a VM). If a VirtWL device has been
specified (e.g. `--virtwl-device=/dev/wl0`) then sommelier will use this
mechanism by default to establish new channels between the host compositor and
sommelier instances. Data is forwarded between the VirtWL device and the core
Wayland dispatch mechanism using non-blocking I/O multiplexing.

## Shared Memory Drivers

Shared memory allocated inside a container cannot always be shared with the
host compositor. Sommelier provides a shared memory driver option as a
solution for this. What's the most appropriate option depends on the host
compositor and device drivers available for allocating buffers.

### VirtWL

The `virtwl` driver creates a set of intermediate virtwl buffers for each
surface, and copies minimal damaged areas from the client’s standard shared
memory buffers into the virtwl buffers that can be shared with the host
compositor.

### VirtWL-DMABuf

The `virtwl-dmabuf` works the same way as the `virtwl` driver but allocates
buffers that can be shared with the host compositor using the linux_dmabuf
protocol. The benefits of using this driver over the basic `virtwl` driver
are:

* Larger set of supported formats (E.g NV12).
* Host compositor can avoid expensive texture uploads.
* HW overlays can be used for presentation if support by the host compositor.

## Damage Tracking

Shared memory drivers that use intermediate buffers require some form of
damage tracking in order to update intermediate buffers.

### Surface Buffer Queue

Each client surface in sommelier is associated with a buffer queue. Each
buffer in the buffer queue has a region (list of rectangles) that describes
the part of the buffer that is damaged compared to the last frame submitted
by the client. This provides high precision damage tracking across multiple
frames. Each new frame from the client adds damage to existing buffers. When
submitting a frame to the host compositor, the next available buffer is
dequeued and updated to not contain any damage. This is done by copying
contents from the current client buffer into the dequeued buffer.

The client's buffer is released as soon as this copy operation described above
is complete and the client can then reuse the shared memory buffer for another
frame.

Note: It is important to release the buffer immediately as clients don’t
expect it to be held by the compositor for long when using shared memory.

### Back Pressure

Sommelier doesn’t provide any back pressure for when the client is producing
contents faster than the host compositor can consume it. The size of the
buffer queue can as a result grow large. This is not a problem as Xwayland
and other clients handle back pressure themselves using Wayland frame
callbacks or similar mechanism.

## Data Drivers

Socket pairs created inside a container cannot always be shared with the
host compositor. Sommelier provides a data driver option as a solution
for this.

### VirtWL

The `virtwl` driver creates a special pipe that can be shared with the host
compositor and forwards all data received over this pipe to the client FD.
Forwarding is done using non-blocking I/O multiplexing.

## Flags and Settings

Sommelier has two forms of configuration. Command line flags and environment
variables. Standard practice is to expose each option both as a command line
flag and as an environment variable. Command line flags will always override
the configuration provided by environment variables. This makes it easy to
run sommelier as a systemd service and allow the system-wide configuration
to be overridden using a local user provided systemd override file.

## Density and Scaling

A protocol aware proxy compositor between the client and the host compositor
makes it easier to support Linux programs that lack good HiDPI support.
It can also be used to adjust the scale of contents to support the dynamic
density changes that Chrome OS UI provide, and it gives the user an option
override any density decisions made by the host compositor. For example,
HiDPI aware programs can run at native display resolution, while some older
programs can use half of that resolution.

### Contents Scaling

Contents scaling can be applied to both native wayland clients and X11
clients. It can be controlled using the `--scale=SCALE` flag or
`SOMMELIER_SCALE=SCALE` variable. Where `SCALE` is a display density
multiplier. For example, if the default density is 200 DPI, then using
`--scale=0.5` will result in contents produced for 100 DPI.

### Scale Factor

An optimal scale factor is calculated for Wayland clients based on contents
scale setting and the current host compositor scaling. This allows Wayland
clients to produce contents at an optimal resolution for all combinations of
scaling used by sommelier and the host compositor.

### DPI

An exact value for DPI is calculated by sommelier. However, many Linux
programs expect DPI to be one out of a well known set of values. Sommelier
solves this by adjusting DPI using a set of buckets. For example, given the
set of buckets (72, 96, 160, 240), Sommelier will use 96 as DPI when the
exact value is 112, or 160 when exact value is 188. The DPI buckets that
sommelier should use can be specified with `--dpi=[DPI[,DPI...]]`. Where,
`--dpi=””` will result in sommelier exposing the exact DPI value to clients
(this is the default behaviour).

### XCursor

Sommelier will set `XCURSOR_SIZE` environment variable automatically based on
the contents scale and preferred host compositor scale factor.

## Accelerators

If the host compositor support dynamic handling of keyboard events, then
keyboard shortcuts are forwarded to the Linux program by default. A small set
of shortcuts are expected to be reserved by the host compositor. A list of
reserved shortcuts on Chrome OS can be found
[here](https://chromium.googlesource.com/chromium/src/+/HEAD/ash/accelerators/accelerator_table.h#22).

There’s unfortunately no reliable way to detect if a Linux program handled a
key event or not. This means that all non-reserved shortcuts that the user
want the host compositor to handle needs to be explicitly listed as an
accelerator. For example, on Chrome OS, the launcher can be brought up using
the "launcher" button during normal usage. The "launcher" button event is
forwarded to Linux programs by default so it won’t work when a Linux program
has keyboard focus unless this shortcut is explicitly listed as an accelerator.

Sommelier provides the `--accelerators=ACCELERATORS` flag for this purpose.
`ACCELERATORS` is a comma separated list of accelerators that shouldn’t be
forwarded to the Linux program but instead handled by the host compositor.
Each accelerator can contain a list of modifiers (e.g. `<Control><Alt>`) and
must be followed by an XKB keysym. The `xev` utility can be used to determine
what the XKB keysym is for a specific key. Given the launcher button example
above (which happens to have XKB keysym `Super_L` on the Chromebook Pixel),
`--accelerators=Super_L` needs to be passed to sommelier for the this button to
bring up the application launcher when Linux programs have keyboard focus.

There is also the `--windowed-accelerators=WINDOWED_ACCELERATORS` flag for
accelerators that should be handled by the host compositor when the focused
window is windowed but not while it is fullscreen.

Consistent with other flags, `SOMMELIER_ACCELERATORS` and
`SOMMELIER_WINDOWED_ACCELERATORS` environment variable can
be used as an alternative to the command line flag.

## Examples

Start parent sommelier and use wayland-1 as name of socket to listen on:

```
sommelier --parent --socket=wayland-1
```

Start sommelier that runs weston-terminal with density scale multiplier 1.5:

```
sommelier --scale=1.5 weston-terminal
```

Start sommelier that runs inkscape with density scale multiplier 0.75 and 120
dots per inch (note that -X is specified as inkscape is an X11 client and
requires X11 forwarding):

```
sommelier -X --scale=0.75 --dpi=120 inkscape
```

Start sommelier that runs gedit with some accelerators reserved to the host
compositor instead of being sent to gedit:

```
sommelier --accelerators="<Alt>Bracketright,<Alt>Bracketleft" gedit
```

## Gamepad Support

See [gaming.md](gaming.md).

## Writing unit tests

See [TESTING.md](TESTING.md).
