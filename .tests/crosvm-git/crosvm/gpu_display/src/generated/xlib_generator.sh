#!/bin/bash
# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd "${0%/*}"

cat >xlib.rs <<EOF
// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Generated using ./xlib_generator.sh

#![allow(clippy::upper_case_acronyms)]

#[link(name = "X11")]
extern "C" {}

#[link(name = "Xext")]
extern "C" {}

EOF

bindgen --no-layout-tests --no-derive-debug \
  --allowlist-function XAllocSizeHints \
  --allowlist-function XBlackPixelOfScreen \
  --allowlist-function XClearWindow \
  --allowlist-function XCloseDisplay \
  --allowlist-function XConnectionNumber \
  --allowlist-function XCreateGC \
  --allowlist-function XCreateSimpleWindow \
  --allowlist-function XDefaultDepthOfScreen \
  --allowlist-function XDefaultScreenOfDisplay \
  --allowlist-function XDefaultVisualOfScreen \
  --allowlist-function XDestroyImage \
  --allowlist-function XDestroyWindow \
  --allowlist-function XFlush \
  --allowlist-function XFree \
  --allowlist-function XFreeGC \
  --allowlist-function XGetVisualInfo \
  --allowlist-function XInternAtom \
  --allowlist-function XKeycodeToKeysym \
  --allowlist-function XMapRaised \
  --allowlist-function XNextEvent \
  --allowlist-function XOpenDisplay \
  --allowlist-function XPending \
  --allowlist-function XRootWindowOfScreen \
  --allowlist-function XScreenNumberOfScreen \
  --allowlist-function XSelectInput \
  --allowlist-function XSetWMNormalHints \
  --allowlist-function XSetWMProtocols \
  --allowlist-function XShmAttach \
  --allowlist-function XShmCreateImage \
  --allowlist-function XShmDetach \
  --allowlist-function XShmGetEventBase \
  --allowlist-function XShmPutImage \
  --allowlist-function XShmQueryExtension \
  --allowlist-function XStoreName \
  --allowlist-var 'XK_.*' \
  --allowlist-var ButtonPress \
  --allowlist-var ButtonPressMask \
  --allowlist-var Button1 \
  --allowlist-var Button1Mask \
  --allowlist-var ButtonRelease \
  --allowlist-var ButtonReleaseMask \
  --allowlist-var ClientMessage \
  --allowlist-var Expose \
  --allowlist-var ExposureMask \
  --allowlist-var KeyPress \
  --allowlist-var KeyPressMask \
  --allowlist-var KeyRelease \
  --allowlist-var KeyReleaseMask \
  --allowlist-var MotionNotify \
  --allowlist-var PMaxSize \
  --allowlist-var PMinSize \
  --allowlist-var PointerMotionMask \
  --allowlist-var ShmCompletion \
  --allowlist-var VisualBlueMaskMask \
  --allowlist-var VisualDepthMask \
  --allowlist-var VisualGreenMaskMask \
  --allowlist-var VisualRedMaskMask \
  --allowlist-var VisualScreenMask \
  --allowlist-var ZPixmap \
  --allowlist-type Display \
  --allowlist-type GC \
  --allowlist-type Screen \
  --allowlist-type XShmCompletionEvent \
  --allowlist-type ShmSeg \
  --allowlist-type Visual \
  --allowlist-type Window \
  --allowlist-type XVisualInfo \
  xlib_wrapper.h >>xlib.rs
