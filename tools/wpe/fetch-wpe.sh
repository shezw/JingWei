#!/usr/bin/env bash
# JingWei
# tools wpe fetch-wpe.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly WPE_VERSION="${WPE_VERSION:-2.52.5}"
readonly EXPECTED_VERSION="2.52.5"
readonly WPE_SHA256="bcfc6c91db7659dcf24f6ff79ad27ac1eae1bc61dca0dbfee154706926740b3b"
readonly DOWNLOAD_DIR="${WPE_DOWNLOAD_DIR:-/workspace/wpe/downloads}"
readonly SOURCE_DIR="${WPE_SOURCE_DIR:-/workspace/wpe/src}"
readonly ARCHIVE="${DOWNLOAD_DIR}/wpewebkit-${WPE_VERSION}.tar.xz"
readonly RELEASE_URL="https://wpewebkit.org/releases/wpewebkit-${WPE_VERSION}.tar.xz"
readonly VERSION_FILE="${SOURCE_DIR}/.jingwei-wpe-version"

if [[ "${WPE_VERSION}" != "${EXPECTED_VERSION}" ]]; then
    echo "Unsupported WPE_VERSION=${WPE_VERSION}; update the pinned checksum before changing versions." >&2
    exit 2
fi

mkdir -p "${DOWNLOAD_DIR}" "${SOURCE_DIR}"

if [[ -f "${VERSION_FILE}" ]] && [[ "$(<"${VERSION_FILE}")" == "${WPE_VERSION}" ]]; then
    echo "WPE WebKit ${WPE_VERSION} source is already available at ${SOURCE_DIR}."
    exit 0
fi

if find "${SOURCE_DIR}" -mindepth 1 -maxdepth 1 -print -quit | grep -q .; then
    echo "Source directory is not empty: ${SOURCE_DIR}" >&2
    echo "Use a fresh named volume or clear it explicitly before changing WPE versions." >&2
    exit 3
fi

if [[ ! -f "${ARCHIVE}" ]]; then
    curl --fail --location --retry 3 --output "${ARCHIVE}" "${RELEASE_URL}"
fi

printf '%s  %s\n' "${WPE_SHA256}" "${ARCHIVE}" | sha256sum --check -
tar --extract --xz --file "${ARCHIVE}" --directory "${SOURCE_DIR}" --strip-components=1
printf '%s\n' "${WPE_VERSION}" > "${VERSION_FILE}"

echo "Prepared WPE WebKit ${WPE_VERSION} source at ${SOURCE_DIR}."
