#!/bin/bash

set -e
set -o xtrace

CROSS_FILE=/cross_file-"$CROSS".txt

# We need to control the version of llvm-config we're using, so we'll
# tweak the cross file or generate a native file to do so.
if test -n "$LLVM_VERSION"; then
    LLVM_CONFIG="llvm-config-${LLVM_VERSION}"
    echo -e "[binaries]\nllvm-config = '`which $LLVM_CONFIG`'" > native.file
    if [ -n "$CROSS" ]; then
        sed -i -e '/\[binaries\]/a\' -e "llvm-config = '`which $LLVM_CONFIG`'" $CROSS_FILE
    fi
    $LLVM_CONFIG --version
else
    rm -f native.file
    touch native.file
fi

# cross-xfail-$CROSS, if it exists, contains a list of tests that are expected
# to fail for the $CROSS configuration, one per line. you can then mark those
# tests in their meson.build with:
#
# test(...,
#      should_fail: meson.get_cross_property('xfail', '').contains(t),
#     )
#
# where t is the name of the test, and the '' is the string to search when
# not cross-compiling (which is empty, because for x86_64 everything is
# expected to pass).
if [ -n "$CROSS" ]; then
    CROSS_XFAIL=.gitlab-ci/cross-xfail-"$CROSS"
    if [ -s "$CROSS_XFAIL" ]; then
        sed -i \
            -e '/\[properties\]/a\' \
            -e "xfail = '$(tr '\n' , < $CROSS_XFAIL)'" \
            "$CROSS_FILE"
    fi
fi

# Only use GNU time if available, not any shell built-in command
case $CI_JOB_NAME in
    # strace and wine don't seem to mix well
    # ASAN leak detection is incompatible with strace
    debian-mingw32-x86_64|*-asan*)
        if test -f /usr/bin/time; then
            MESON_TEST_ARGS+=--wrapper=$PWD/.gitlab-ci/meson/time.sh
        fi
        ;;
    *)
        if test -f /usr/bin/time -a -f /usr/bin/strace; then
            MESON_TEST_ARGS+=--wrapper=$PWD/.gitlab-ci/meson/time-strace.sh
        fi
        ;;
esac

RET=0
RESULTS_DIR=$(pwd)/results/${TEST_SUITE:-build}
rm -rf _build

meson setup _build --native-file=native.file \
    --wrap-mode=${WRAP_DEBUG:-nofallback} \
    ${CROSS+--cross "$CROSS_FILE"} \
    -D prefix=$(pwd)/install \
    -D libdir=lib \
    -D buildtype=${BUILDTYPE:-debug} \
    -D c_args="$(echo -n $C_ARGS)" \
    -D cpp_args="$(echo -n $CPP_ARGS)" \
    ${DRI_LOADERS} \
    ${GALLIUM_ST} \
    -D tests=true \
    -D render-server=true \
    -D render-server-worker=process \
    -D venus=true \
    -Dtracing=${TRACING_BACKEND:-none} \
    --fatal-meson-warnings \
    ${EXTRA_OPTION} && \
pushd _build && \
meson compile -j ${FDO_CI_CONCURRENT:-4} || {
    RET=$?
    mkdir -p ${RESULTS_DIR}
    mv -f meson-logs/* ${RESULTS_DIR}/
    popd
    exit ${RET}
}
meson install

if [ -n "${TEST_SUITE}" ]; then
    VRENDTEST_USE_EGL_SURFACELESS=1 meson test --num-processes ${FDO_CI_CONCURRENT:-4} || RET=$?
    mkdir -p ${RESULTS_DIR}
    mv -f meson-logs/testlog.txt ${RESULTS_DIR}/
fi

popd
exit ${RET}

