#!/bin/bash

root=""

if [ "x$1" != "x" ]; then
    root="$1"
fi     
cd "$root/perfetto"
GN_ARGS="is_debug=false use_custom_libcxx=false"
tools/install-build-deps --ui
tools/gn gen out/dist --args="${GN_ARGS}" --check
tools/ninja -C out/dist traced traced_probes perfetto trace_to_text ui trace_processor_shell
ui/run-dev-server out/dist/
