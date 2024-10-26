# Writing Sommelier tests

[Sommelier](README.md)'s main function is passing messages between other applications.

* For [peer Wayland](README.md#peer-sommelier) instances, that's the host compositor on one hand, and a
  Wayland client on the other.
* [X11 instances](README.md#x11-sommelier) are similar, except the Wayland client is Xwayland. Sommelier
  also connects to Xwayland using the X11 protocol, in Sommelier's role as
  the X Window Manager.

Most test cases can be implemented by faking input over one of three
connections, and asserting that Sommelier produces appropriate output over
another connection. The following sections describe how.

For tests applicable to peer Wayland instances, your test fixture should
inherit from WaylandTestBase. For X11 instances, inherit from X11TestBase.

## Host compositor -> Sommelier (Wayland events)

Wayland events are received on Wayland client objects, so first you need to
create one. The specifics will vary depending on the protocol, but generally
you'll want to first obtain a global, and possibly request another object from
it.

As we don't have a real Wayland server, WaylandTestBase::Connect() advertises
globals instead. For example, xdg_wm_base is advertised as follows:

```
sl_registry_handler(..., "xdg_wm_base",
    XDG_WM_BASE_GET_XDG_SURFACE_SINCE_VERSION);
```

If the global you need isn't there, feel free to add it.

Once the "server" advertises the global, find it in `ctx` and use Wayland
requests as normal to create other client objects. For example, to create a
`wl_surface`, call `wl_compositor_create_surface(ctx.compositor->internal)`.

The canonical way to fake receiving a Wayland event from the host compositor
is to simply call the event handler directly, using the following pattern:

```
HostEventHandler(proxy)->eventname(nullptr, params...);
```

Where `proxy` is any Wayland client object such as a `wl_surface`,
`eventname` is the event your test pretends to receive, and the parameters
vary depending on the Wayland event.

If you get template-related errors when using HostEventHandler, fix them by
adding a
[MAP_STRUCT_TO_LISTENER()](https://crsrc.org/o/src/platform2/vm_tools/sommelier/testing/sommelier-test-util.h?q=MAP_STRUCT_TO_LISTENER)
mapping for the Wayland client object's type, following existing examples.

## Sommelier -> host compositor (Wayland requests)

The Sommelier build generates C++ "shim" classes for each Wayland client
object, which simply wrap libwayland's generated functions. For example,
`XdgSurfaceShim::set_window_geometry` calls `xdg_surface_set_window_geometry`.
We are moving towards using these shim classes throughout Sommelier instead of
calling libwayland directly. This lets us use gMock to replace those shims in
tests, and verify that the expected Wayland request was sent.

The mock classes are also created by codegen. The test must simply install
them, overriding the real implementation:

```
// In fixture class declaration:
NiceMock<MockXdgSurfaceShim> mock_xdg_surface_shim_;
...
// In SetUp():
set_xdg_surface_shim(&mock_xdg_surface_shim_);
```

Then verify each request using standard gMock syntax:

```
EXPECT_CALL(mock_xdg_surface_shim_, set_window_geometry(<arguments go here>))
    .Times(1);
```

## Wayland client -> Sommelier (Wayland requests)

_For X11 instances, the Wayland client is Xwayland._

Create an instance of FakeWaylandClient. This creates an actual Wayland client
and connects it to Sommelier, such that it can send Wayland requests as per
usual, and Sommelier will handle them as if they'd been sent by a real client.

FakeWaylandClient exposes methods to have the client perform various tasks,
which are then driven by the test case. See
`FakeWaylandClient::CreateSurface()` for an example. Feel free to add more as
your test case requires. If the new method is very specific to your test case,
consider subclassing FakeWaylandClient.

* When creating a new method, remember to call `wl_display_flush()` so that
  Sommelier actually receives the sent request.
* X11TestBase already creates a FakeWaylandClient representing Xwayland. See
  `X11TestBase::xwayland`.

## Sommelier -> Wayland client (Wayland events)

_For X11 instances, the Wayland client is Xwayland._

Same as "Sommelier -> host compositor" above, but the method to verify will
begin with `send_`. For example, to verify that Sommelier sends
`xdg_surface.configure()` to the client, write:

```
EXPECT_CALL(mock_xdg_surface_shim_, send_configure(<arguments go here>))
    .Times(1);
```

## Sommelier -> Xwayland (X11 requests)

The XcbShim class wraps libxcb functions. We're moving towards calling all xcb
functions through this shim. X11TestBase creates a MockXcbShim which can be
used to verify these calls.

For example, to verify that Sommelier sends a ConfigureWindow request, write:

```
EXPECT_CALL(xcb, configure_window(<arguments go here>))
    .Times(1);
```

## Xwayland -> Sommelier (X11 events)

For now, to fake receiving an X11 event, simply call the corresponding handler
function directly. For example, `sl_handle_client_message()` or
`sl_handle_map_request()`. We might make this nicer in future by exposing a
single function to handle all X11 events.
