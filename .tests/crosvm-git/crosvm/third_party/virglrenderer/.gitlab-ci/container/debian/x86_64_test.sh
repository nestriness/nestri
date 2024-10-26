#!/usr/bin/env bash

set -ex

alias curl="curl -L --retry 4 -f --retry-all-errors --retry-delay 60"
MESA_CI_PROJECT_DIR="/builds/${MESA_PROJECT_PATH}"
mkdir -p ${MESA_CI_PROJECT_DIR}
cd ${MESA_CI_PROJECT_DIR}

# Deploy Mesa CI artifacts
MESA_ARCHITECTURE="x86_64"
MESA_CONFIGURATION="default"
MESA_BUILDTYPE="debugoptimized"
MESA_TARBALL="mesa-${MESA_ARCHITECTURE}-${MESA_CONFIGURATION}-${MESA_BUILDTYPE}.tar.zst"
MESA_CI_ARTIFACTS_URL="https://${STORAGE_HOST}/artifacts/${MESA_PROJECT_PATH}/${MESA_PIPELINE_ID}/${MESA_TARBALL}"
if curl -s -I ${MESA_CI_ARTIFACTS_URL}; then
    curl ${MESA_CI_ARTIFACTS_URL} -o - | tar -xv --zstd
else
    echo -e "\e[31mThe Mesa artifacts has expired, please update to newer Mesa pipeline!\e[0m"
    apt-get update && apt-get -y install jq
    MESA_PROJECT_PATH_ESCAPED=$(echo "$MESA_PROJECT_PATH" | sed 's|/|%2F|')
    MESA_PROJECT_ID=$(curl -s "${CI_API_V4_URL}/projects/${MESA_PROJECT_PATH_ESCAPED}" -o - | jq -c '.id')
    FALLBACK_PAGE=1
    while :
    do
        MESA_JOB_ID=$(curl -s "${CI_API_V4_URL}/projects/${MESA_PROJECT_ID}/pipelines/${MESA_PIPELINE_ID}/jobs?per_page=100&page=${FALLBACK_PAGE}&scope=success" -o - \
          | jq -c '.[] | select(.name == "debian-testing") | .id')
        if [ ! -z "${MESA_JOB_ID}" ]; then
            break
        fi
        if [ $FALLBACK_PAGE -ge 10 ]; then
            echo -e "\e[31mUnable to find the debian-testing job!\e[0m"
            exit 1
        fi
        FALLBACK_PAGE=$((FALLBACK_PAGE+1))
    done
    MESA_CI_ARTIFACTS_URL="${CI_API_V4_URL}/projects/${MESA_PROJECT_ID}/jobs/${MESA_JOB_ID}/artifacts/artifacts/install.tar"
    unset MESA_JOB_ID
    curl ${MESA_CI_ARTIFACTS_URL} -o - | tar -xv
fi

# Directory used by crosvm-runner.sh
export SCRIPTS_DIR=$(pwd)/install
. ${SCRIPTS_DIR}/setup-test-env.sh

# Overwrite Mesa CI's virglrenderer binaries with self built versions
cp -a ${CI_PROJECT_DIR}/install/bin/virgl_test_server /usr/local/bin/
cp -a ${CI_PROJECT_DIR}/install/libexec/virgl_render_server /usr/local/libexec/
cp -a ${CI_PROJECT_DIR}/install/lib/libvirglrenderer.so* /usr/local/lib/

if [ "${VK_DRIVER}" = "virtio" ] || [ "${GALLIUM_DRIVER}" = "virgl" ]; then
    #
    # Run the tests on virtual platform (virgl/crosvm)
    #
    cp -a ${CI_PROJECT_DIR}/.gitlab-ci/expectations/virt/*.txt install/
    cp -a ${CI_PROJECT_DIR}/.gitlab-ci/expectations/virt/*.toml install/

    #
    # crosvm-runner.sh depends on resources from ${CI_PROJECT_DIR}/install,
    # but their actual location is ${MESA_CI_PROJECT_DIR}/install, hence
    # let's fix this using a bind mount.
    #
    mv ${CI_PROJECT_DIR}/install ${CI_PROJECT_DIR}/install-orig
    mkdir ${CI_PROJECT_DIR}/install
    mount --bind install ${CI_PROJECT_DIR}/install

    export LD_LIBRARY_PATH="${CI_PROJECT_DIR}/install/lib"
    set +e

    if [ -z "${DEQP_SUITE}" ]; then
        if [ -z "${PIGLIT_REPLAY_DESCRIPTION_FILE}" ]; then
            FDO_CI_CONCURRENT=${FORCE_FDO_CI_CONCURRENT:-FDO_CI_CONCURRENT} \
                install/crosvm-runner.sh install/piglit/piglit-runner.sh
        else
            FDO_CI_CONCURRENT=${FORCE_FDO_CI_CONCURRENT:-FDO_CI_CONCURRENT} \
                install/crosvm-runner.sh install/piglit/piglit-traces.sh
        fi
    else
        install/deqp-runner.sh
    fi

    RET=$?

    # Cleanup
    umount ${CI_PROJECT_DIR}/install && \
    rmdir ${CI_PROJECT_DIR}/install && \
    mv ${CI_PROJECT_DIR}/install-orig ${CI_PROJECT_DIR}/install
else
    #
    # Run the tests on host platform (virpipe/vtest)
    #
    cp -a ${CI_PROJECT_DIR}/.gitlab-ci/expectations/host/*.txt install/
    cp -a ${CI_PROJECT_DIR}/.gitlab-ci/expectations/host/*.toml install/

    export LIBGL_ALWAYS_SOFTWARE="true"
    set +e

    if [ -z "${DEQP_SUITE}" ]; then
        PIGLIT_RUNNER_OPTIONS="--timeout 180" \
            install/piglit/piglit-runner.sh
    else
        DEQP_EXPECTED_RENDERER=virgl \
        WAFFLE_PLATFORM="surfaceless_egl" \
        SANITY_MESA_VERSION_CMD=wflinfo \
        HANG_DETECTION_CMD= \
        EGL_PLATFORM=surfaceless \
            install/deqp-runner.sh
    fi

    RET=$?
fi

mv -f results ${CI_PROJECT_DIR}/
exit ${RET}
