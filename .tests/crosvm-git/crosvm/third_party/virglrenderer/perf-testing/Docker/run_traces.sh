# This script is to be run on the KVM guest

set -ex

mkdir /waffle
mount -t virtiofs waffle-tag /waffle

mkdir /apitrace
mount -t virtiofs apitrace-tag /apitrace

mkdir /traces-db
mount -t virtiofs traces-db-tag /traces-db

mkdir /perfetto
mount -t virtiofs perfetto-tag /perfetto

echo 3 > /proc/sys/kernel/printk

export PATH="/apitrace/build:$PATH"
export PATH="/waffle/build/bin:$PATH"
export LD_LIBRARY_PATH="/waffle/build/lib:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
export EGL_PLATFORM="surfaceless"
export WAFFLE_PLATFORM="surfaceless_egl"
export MESA_GL_VERSION_OVERRIDE="4.5"
export DISPLAY=

# Comment out any other sources, so it only syncs to the host via PTP
sed -i '/pool/s/^/#/' /etc/chrony/chrony.conf
echo refclock PHC /dev/ptp0 poll 1 dpoll -2 offset 0 >> /etc/chrony/chrony.conf
echo cmdport 0 >> /etc/chrony/chrony.conf
echo bindcmdaddress / >> /etc/chrony/chrony.conf

mkdir -p /run/chrony
time chronyd -q   # Initial synchronization, will take some time
chronyd      # Keep clocks in sync

# Get trace cached
trace_no_ext=$(cat /traces-db/current_trace)
if [ "x$trace_no_ext" = "x" ]; then
    echo "No trace given, bailing out"
    exit 1 
fi

command=$(cat /traces-db/command)
echo command=$command

WAIT=
RECORD=
benchmark_loops=0
perfetto_loops=10

for c in $command; do

  val=(${c//=/ })
   case "${val[0]}" in
   benchmark)
     benchmark_loops=${val[1]}
     ;;

   perfetto)
     perfetto_loops=${val[1]}
     ;;

   wait-after-frame)
     WAIT="--wait-after-frame"
     ;;

   record-frame)
     RECORD="--snapshot"
     ;;

   esac
done

if [ -e /traces-db/wait_after_frame ]; then
   WAIT=-wait-after-frame
fi

trace="/traces-db/${trace_no_ext}.trace"
datadir="/traces-db/${trace_no_ext}-out"
trace_base=$(basename ${trace_no_ext})
guest_perf="$datadir/${trace_base}-guest.perfetto"

cat "$trace" > /dev/null

# To keep Perfetto happy
echo 0 > /sys/kernel/debug/tracing/tracing_on
echo nop > /sys/kernel/debug/tracing/current_tracer

echo "Guest:"
wflinfo --platform surfaceless_egl --api gles2 -v

if [ "x$perfetto_loops" != "x" -a "x$perfetto_loops" != "x0" ]; then
    /perfetto/out/dist/traced &
    /perfetto/out/dist/traced_probes &
    sleep 1

    /perfetto/out/dist/perfetto --txt -c /usr/local/perfetto-guest.cfg -o "$guest_perf" --detach=mykey
    sleep 1

    # The first virtio-gpu event has to be captured in the guest, so we correlate correctly to the host event

    echo "Replaying for Perfetto:"
    eglretrace --benchmark --singlethread --loop=$perfetto_loops $WAIT --headless "$trace"
    sleep 1

    /perfetto/out/dist/perfetto --attach=mykey --stop
    chmod a+rw "$guest_perf"
fi

if [ "x$benchmark_loops" != "x0" ]; then
   echo "Measuring rendering times:"
   eglretrace --benchmark --loop=$benchmark_loops --headless "$trace"
fi

if [ "x$RECORD" != "x" ]; then
   eglretrace --snapshot frame --snapshot-prefix=${datadir}/ --headless "$trace"
fi
