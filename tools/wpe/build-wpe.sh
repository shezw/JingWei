#!/usr/bin/env bash
# JingWei
# tools wpe build-wpe.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly WPE_VERSION="${WPE_VERSION:-2.52.5}"
readonly WPE_PROFILE="${WPE_PROFILE:-container}"
readonly BUILD_ROOT="${WPE_BUILD_ROOT:-/workspace/wpe/build}"
readonly BUILD_DIR="${BUILD_ROOT}/${WPE_VERSION}-${WPE_PROFILE}"
readonly BUILD_JOBS="${WPE_BUILD_JOBS:-3}"

"/workspace/JingWei/tools/wpe/fetch-wpe.sh"
"/workspace/JingWei/tools/wpe/configure-wpe.sh"

cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS}"
cmake --install "${BUILD_DIR}"
