#!/usr/bin/env bash
# JingWei
# tools container build-jingwei.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly SOURCE_DIR="${JINGWEI_SOURCE_DIR:-/workspace/JingWei}"
readonly BUILD_DIR="${JINGWEI_BUILD_DIR:-/workspace/jingwei-build}"
readonly BUILD_TYPE="${JINGWEI_BUILD_TYPE:-RelWithDebInfo}"
readonly BUILD_JOBS="${JINGWEI_BUILD_JOBS:-3}"

cmake \
    -S "${SOURCE_DIR}" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache

cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
