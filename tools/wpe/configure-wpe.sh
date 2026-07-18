#!/usr/bin/env bash
# JingWei
# tools wpe configure-wpe.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly WPE_VERSION="${WPE_VERSION:-2.52.5}"
readonly WPE_PROFILE="${WPE_PROFILE:-container}"
readonly SOURCE_DIR="${WPE_SOURCE_DIR:-/workspace/wpe/src}"
readonly BUILD_ROOT="${WPE_BUILD_ROOT:-/workspace/wpe/build}"
readonly PREFIX_ROOT="${WPE_PREFIX_ROOT:-/workspace/wpe/prefix}"
readonly BUILD_DIR="${BUILD_ROOT}/${WPE_VERSION}-${WPE_PROFILE}"
readonly PREFIX_DIR="${PREFIX_ROOT}/${WPE_VERSION}-${WPE_PROFILE}"
readonly BUILD_TYPE="${WPE_BUILD_TYPE:-Release}"

common_options=(
    -DPORT=WPE
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}"
    -DCMAKE_C_COMPILER_LAUNCHER=ccache
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    -DDBUS_PROXY_EXECUTABLE=/usr/bin/xdg-dbus-proxy
    -DENABLE_DOCUMENTATION=OFF
    -DENABLE_INTROSPECTION=OFF
    -DENABLE_JOURNALD_LOG=OFF
    -DENABLE_WPE_LEGACY_API=OFF
    -DENABLE_WPE_PLATFORM=ON
    -DENABLE_WPE_PLATFORM_HEADLESS=ON
    -DENABLE_WPE_PLATFORM_WAYLAND=OFF
    -DENABLE_WPE_QT_API=OFF
    -DUSE_LIBBACKTRACE=OFF
)

case "${WPE_PROFILE}" in
    container)
        profile_options=(
            -DENABLE_ENCRYPTED_MEDIA=OFF
            -DENABLE_GAMEPAD=OFF
            -DENABLE_MEDIA_RECORDER=OFF
            -DENABLE_MEDIA_STREAM=OFF
            -DENABLE_SPEECH_SYNTHESIS=OFF
            -DENABLE_VIDEO=OFF
            -DENABLE_WEB_AUDIO=OFF
            -DENABLE_WEB_CODECS=OFF
            -DENABLE_WEB_RTC=OFF
            -DENABLE_WPE_PLATFORM_DRM=OFF
            -DUSE_ATK=OFF
            -DUSE_FLITE=OFF
            -DUSE_GBM=OFF
            -DUSE_GSTREAMER=OFF
            -DUSE_LIBDRM=ON
        )
        ;;
    drm)
        profile_options=(
            -DENABLE_WPE_PLATFORM_DRM=ON
            -DUSE_GBM=ON
            -DUSE_LIBDRM=ON
        )
        ;;
    *)
        echo "Unknown WPE_PROFILE=${WPE_PROFILE}; expected container or drm." >&2
        exit 2
        ;;
esac

cmake \
    -S "${SOURCE_DIR}" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    "${common_options[@]}" \
    "${profile_options[@]}"

echo "Configured WPE WebKit ${WPE_VERSION} (${WPE_PROFILE}) at ${BUILD_DIR}."
