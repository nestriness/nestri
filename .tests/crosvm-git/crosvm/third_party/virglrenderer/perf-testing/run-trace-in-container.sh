#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is to be run on the KVM host, outside the container


#set -ex

# grab the pwd before changing it to this script's directory
pwd="${PWD}"
root_dir="$(pwd)"

cd "${0%/*}"

mesa_src=""
virgl_src=""
traces_db=""
kernel_src=""
benchmark_loops=0
perfetto_loops=10
wait_after_frame=

print_help() {
  echo "Run GL trace with perfetto"
  echo "Usage mesa_wrapper.sh [options]"
  echo ""
  echo "  --root, -r       Path to a root directory that contains the sources for mesa, virglrenderer, and the trace-db"
  echo "  --mesa, -m       Path to Mesa source code (overrides path generated from root)"
  echo "  --virgl, -v      Path to virglrenderer source code (overrides path generated from root)"
  echo "  --kernel, -k     Path to Linux kernel source code"
  echo "  --traces-db, -d  Path to the directory containing the traces (overrides path generated from root)"
  echo "  --trace, -t      Trace to be run (path relative to traces-db) (required)"
  echo "  --benchmark, -b  Number of times the last frame should be run for benchmarking (default 0=disabled)"
  echo "  --perfetto, -p   Number of times the last frame should be loop for perfetto (default 10; 0=run trace normally)"
  echo "  --snapshot, -s   Make per-frame snapshots"
  echo "  --debug          Enable extra logging"
  echo ""
  echo "  --help, -h       Print this help"
}

command=""

while [ -n "$1" ] ; do
    case "$1" in

        --root|-r)
            root_dir="$2"
            shift
            ;;
        
        --mesa|-m)
            mesa_src="$2"
            shift
            ;;
        
        --virgl|-v)
            virgl_src="$2"
            shift
            ;;
        
        --traces-db|-d)
            traces_db="$2"
            shift
            ;;
        
        --kernel|-k)
            kernel_src="$2"
            shift
            ;;
        
        --help|-h)
            print_help
            exit
            ;;
        
        --trace|-t)
            trace="$2"
            shift
            ;;

        --benchmark|-b)
            command="$command -b $2"
            shift
            ;;

        --perfetto|-p)
            command="$command -p $2"
            shift
            ;;

  	--wait-after-frame|-w)
            command="$command -w"
	    ;;

        --snapshot|-s)
           command="$command -s"
           ;;

        --debug)
           command="$command --debug"
           ;;
        *)
            echo "Unknown option '$1' given, run with option --help to see supported options"
            exit
            ;;
    esac
    shift
done

if [ "x$mesa_src" = "x" ] ; then
    mesa_src="$root_dir/mesa"
fi

if [ "x$virgl_src" = "x" ] ; then
    virgl_src="$root_dir/virglrenderer"
fi

if [ "x$traces_db" = "x" ] ; then
    traces_db="$root_dir/traces-db"
fi

can_run=1

if [ "x$trace" = "x" ]; then
   echo "No trace given"  >&2;
   can_run=0
fi

if [ "x$mesa_src" = "x" ]; then
  echo "no mesa src dir given"  >&2;
  can_run=0
fi

if [ ! -d "$mesa_src/src/mesa" ]; then
  echo "mesa src dir '$mesa_src' is not a mesa source tree"  >&2;
  can_run=0
fi

if [ "x$virgl_src" = "x" ]; then
  echo "no virglrenderer src dir given"  >&2;
  can_run=0
fi

if [ ! -d "$virgl_src/vtest" ]; then
  echo "virglrenderer src dir '$virgl_src' is not a virglrenderer source tree"  >&2;
  can_run=0
fi

if [ "x$traces_db" = "x" ]; then
  echo "no traces_db dir given"  >&2;
  can_run=0
fi

if [ ! -f "$traces_db/$trace" ]; then
   echo "Given trace file '$trace' doesn't exist in traces db dir '$traces_db'"  >&2;
   # check whether the trace has been given with a path relative to the root dir
   # that can be removed
   trace=${trace#*/}
   echo "Trying $traces_db/$trace"  >&2;
   if [ ! -f "$traces_db/$trace" ]; then
      echo "Given trace file '$trace' not found "  >&2;
      can_run=0
   fi
fi

if [ "x$can_run" = "x0" ]; then
  echo "Missing or command line options with errors were given"  >&2;
  exit 1
fi

re='^[0-9]+$'
if ! [[ 1$benchmark_loops =~ $re ]] ; then
   echo "error: benchmark_loops '" $benchmark_loops "'is not a number" >&2;
   exit 1
fi

if ! [[ 1$perfetto_loops =~ $re ]] ; then
   echo "error: perfetto_loops '" $perfetto_loops "' is not a number" >&2;
   exit 1
fi

echo "command=$command"

docker run -it --rm \
    --privileged \
    --ipc=host \
    -v /dev/log:/dev/log \
    -v /dev/vhost-net:/dev/vhost-net \
    -v /sys/kernel/debug:/sys/kernel/debug \
    -v "$mesa_src":/mesa \
    -v "$virgl_src":/virglrenderer \
    -v "$traces_db":/traces-db \
    -v "$kernel_src":/kernel \
    --volume "$pwd":/wd \
    --workdir /wd \
    mesa \
    -t "$trace" \
    $command
