# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

project('sommelier', 'c', 'cpp', default_options: ['cpp_std=c++17'])

#===============#
# Configuration #
#===============#

includes = []
cpp_args = []

#===============#
# Wayland Stuff #
#===============#

wl_scanner = find_program('wayland-scanner')

wl_generators = [
  generator(
    wl_scanner,
    output: '@BASENAME@-code.c',
    arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
  ),
  generator(
    wl_scanner,
    output: '@BASENAME@-client-protocol.h',
    arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
  ),
  generator(
    wl_scanner,
    output: '@BASENAME@-server-protocol.h',
    arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
  ),
]

wl_protocols = [
  'protocol/aura-shell.xml',
  'protocol/drm.xml',
  'protocol/fractional-scale-v1.xml',
  'protocol/gaming-input-unstable-v2.xml',
  'protocol/gtk-shell.xml',
  'protocol/idle-inhibit-unstable-v1.xml',
  'protocol/keyboard-extension-unstable-v1.xml',
  'protocol/linux-dmabuf-unstable-v1.xml',
  'protocol/linux-explicit-synchronization-unstable-v1.xml',
  'protocol/pointer-constraints-unstable-v1.xml',
  'protocol/relative-pointer-unstable-v1.xml',
  'protocol/stylus-unstable-v2.xml',
  'protocol/tablet-unstable-v2.xml',
  'protocol/text-input-crostini-unstable-v1.xml',
  'protocol/text-input-unstable-v1.xml',
  'protocol/text-input-extension-unstable-v1.xml',
  'protocol/text-input-x11-unstable-v1.xml',
  'protocol/viewporter.xml',
  'protocol/xdg-output-unstable-v1.xml',
  'protocol/xdg-shell.xml',
]

wl_outs = []

foreach p : wl_protocols
  foreach g : wl_generators
    wl_outs += g.process(p)
  endforeach
endforeach

#===============#
# Codegen Shims #
#===============#

# Ensure jinja2 exists within the python3 install.
if run_command('python3', '-c', 'import jinja2', check: false).returncode() != 0
  error('python3 jinja2 missing')
endif

python_program = find_program('gen-shim.py')
shim_generator = generator(
  python_program,
  output: ['@BASENAME@-shim.cc', '@BASENAME@-shim.h', 'mock-@BASENAME@-shim.h'],
  arguments: ['@INPUT@', '@BUILD_DIR@'],
)

shim_outs = []
foreach p : wl_protocols
  shim_outs += shim_generator.process(p)
endforeach

#==========#
# Perfetto #
#==========#

tracing_sources = []
tracing_dependencies = []

if get_option('tracing')
  tracing_dependencies = [
    dependency('threads'),
    dependency('perfetto'),
  ]
  cpp_args += '-DPERFETTO_TRACING'
endif

#=================#
# Gamepad support #
#=================#

gamepad_sources = []
gamepad_testing = []
gamepad_dependencies = []
if get_option('gamepad')
  gamepad_sources = [
    'sommelier-gaming.cc',
    'libevdev/libevdev-shim.cc',
  ]
  gamepad_testing = [
    'sommelier-gaming-test.cc',
  ]
  gamepad_dependencies = [
    dependency('libevdev'),
  ]
  cpp_args += '-DGAMEPAD_SUPPORT'
endif

#===============#
# Quirks config #
#===============#

quirks_sources = []
quirks_testing = []
quirks_dependencies = []
if get_option('quirks')
  # Generate protocol buffer sources
  protoc = find_program('protoc')
  gen = generator(
    protoc,
    output: ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
    arguments: [
      '--proto_path=@CURRENT_SOURCE_DIR@',
      '--cpp_out=@BUILD_DIR@',
      '@INPUT@',
    ],
  )
  quirks_proto = gen.process(
    'quirks/quirks.proto',
    preserve_path_from: meson.current_source_dir(),
  )

  quirks_sources = (
    [
      'quirks/sommelier-quirks.cc',
    ] + quirks_proto
  )

  quirks_testing = (
    [
      'quirks/sommelier-quirks-test.cc',
    ] + quirks_proto
  )

  quirks_dependencies = [
    dependency('protobuf'),
  ]
  cpp_args += '-DQUIRKS_SUPPORT'
endif

#===========#
# Log level #
#===========#

cpp_args += '-DLOG_LEVEL=' + get_option('log_level').to_string()

#===========#
# Sommelier #
#===========#

if get_option('commit_loop_fix')
  cpp_args += '-DCOMMIT_LOOP_FIX'
endif

sommelier_defines = [
  '-D_GNU_SOURCE',
  '-DWL_HIDE_DEPRECATED',
  '-DXWAYLAND_PATH="' + get_option('xwayland_path') + '"',
  '-DXWAYLAND_GL_DRIVER_PATH="' + get_option('xwayland_gl_driver_path') + '"',
  '-DFRAME_COLOR="' + get_option('frame_color') + '"',
  '-DDARK_FRAME_COLOR="' + get_option('dark_frame_color') + '"',
]

testing_defines = []
if (get_option('with_tests'))
  testing_defines += '-DWITH_TESTS'
endif

libsommelier = static_library(
  'sommelier',
  sources: [
    'compositor/sommelier-compositor.cc',
    'compositor/sommelier-dmabuf-sync.cc',
    'compositor/sommelier-drm.cc',
    'compositor/sommelier-formats.cc',
    'compositor/sommelier-linux-dmabuf.cc',
    'compositor/sommelier-mmap.cc',
    'compositor/sommelier-shm.cc',
    'sommelier-ctx.cc',
    'sommelier-data-device-manager.cc',
    'sommelier-display.cc',
    'sommelier-fractional-scale.cc',
    'sommelier-gtk-shell.cc',
    'sommelier-global.cc',
    'sommelier-idle-inhibit-manager.cc',
    'sommelier-inpututils.cc',
    'sommelier-logging.cc',
    'sommelier-output.cc',
    'sommelier-pointer-constraints.cc',
    'sommelier-relative-pointer-manager.cc',
    'sommelier-scope-timer.cc',
    'sommelier-seat.cc',
    'sommelier-shell.cc',
    'sommelier-stylus-tablet.cc',
    'sommelier-subcompositor.cc',
    'sommelier-text-input.cc',
    'sommelier-timing.cc',
    'sommelier-tracing.cc',
    'sommelier-transform.cc',
    'sommelier-util.cc',
    'sommelier-viewporter.cc',
    'sommelier-xdg-shell.cc',
    'sommelier-xshape.cc',
    'sommelier.cc',
    'sommelier-window.cc',
    'virtualization/virtwl_channel.cc',
    'virtualization/virtgpu_channel.cc',
    'xcb/xcb-shim.cc',
  ] + wl_outs + tracing_sources + gamepad_sources + quirks_sources + shim_outs,
  dependencies: [
    meson.get_compiler('cpp').find_library('m'),
    dependency('gbm'),
    dependency('libdrm'),
    dependency('pixman-1'),
    dependency('wayland-client'),
    dependency('wayland-server'),
    dependency('xcb'),
    dependency('xcb-composite'),
    dependency('xcb-shape'),
    dependency('xcb-xfixes'),
    dependency('xkbcommon'),
  ] + gamepad_dependencies + tracing_dependencies + quirks_dependencies,
  cpp_args: cpp_args + sommelier_defines + testing_defines,
  include_directories: includes,
)

executable(
  'sommelier',
  install: true,
  sources: [
    'sommelier-main.cc',
  ] + wl_outs,
  link_with: libsommelier,
  cpp_args: cpp_args + sommelier_defines,
  include_directories: includes,
)

if get_option('with_tests')
  testing_includes = [include_directories('testing')]

  sommelier_test = executable(
    'sommelier_test',
    install: true,
    sources: [
      'compositor/sommelier-linux-dmabuf-test.cc',
      'sommelier-test.cc',
      'sommelier-test-main.cc',
      'sommelier-transform-test.cc',
      'sommelier-output-test.cc',
      'sommelier-window-test.cc',
      'sommelier-x11event-test.cc',
      'sommelier-xdg-shell-test.cc',
      'testing/mock-wayland-channel.cc',
      'testing/sommelier-test-util.cc',
      'xcb/fake-xcb-shim.cc',
    ] + wl_outs + shim_outs + gamepad_testing + quirks_testing,
    link_with: libsommelier,
    dependencies: [
      dependency('gtest'),
      dependency('gmock'),
      dependency('pixman-1'),
    ] + gamepad_dependencies + tracing_dependencies + quirks_dependencies,
    cpp_args: cpp_args + sommelier_defines + testing_defines,
    include_directories: includes + testing_includes,
  )

  test('sommelier_test', sommelier_test)
endif
