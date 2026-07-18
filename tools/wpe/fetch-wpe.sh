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
readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly DOWNLOAD_DIR="${WPE_DOWNLOAD_DIR:-/workspace/wpe/downloads}"
readonly SOURCE_DIR="${WPE_SOURCE_DIR:-/workspace/wpe/src}"
readonly ARCHIVE="${DOWNLOAD_DIR}/wpewebkit-${WPE_VERSION}.tar.xz"
readonly RELEASE_URL="https://wpewebkit.org/releases/wpewebkit-${WPE_VERSION}.tar.xz"
readonly VERSION_FILE="${SOURCE_DIR}/.jingwei-wpe-version"
readonly GIO_UNIX_PATCH="${SCRIPT_DIR}/patches/wpewebkit-2.52.5-gio-unix.patch"
readonly LIBSECCOMP_SCOPED_LINK_PATCH="${SCRIPT_DIR}/patches/wpewebkit-2.52.5-libseccomp-scoped-link.patch"
readonly LIBSECCOMP_POST_TARGET_PATCH="${SCRIPT_DIR}/patches/wpewebkit-2.52.5-libseccomp-post-target.patch"
readonly GIO_UNIX_PATCH_TARGET="${SOURCE_DIR}/Source/WebKit/PlatformWPE.cmake"

apply_backport() {
    local patch_file="$1"
    local patch_name="$2"
    local -a patch_options=(
        --batch
        --silent
        --fuzz=0
        --strip=1
        --directory="${SOURCE_DIR}"
        --input="${patch_file}"
        --reject-file=-
    )

    if [[ ! -f "${patch_file}" ]]; then
        echo "Required WPE backport is missing: ${patch_file}" >&2
        exit 4
    fi

    if patch "${patch_options[@]}" --forward --dry-run >/dev/null 2>&1; then
        patch "${patch_options[@]}" --forward
        echo "Applied WPE ${WPE_VERSION} ${patch_name} backport."
    elif patch "${patch_options[@]}" --reverse --dry-run >/dev/null 2>&1; then
        echo "WPE ${WPE_VERSION} ${patch_name} backport is already applied."
    else
        echo "WPE ${WPE_VERSION} source drifted; ${patch_name} backport applies neither forward nor reverse." >&2
        echo "Refusing to continue with ${patch_file}." >&2
        exit 4
    fi

    if ! patch "${patch_options[@]}" --reverse --dry-run >/dev/null 2>&1; then
        echo "WPE ${WPE_VERSION} ${patch_name} backport verification failed." >&2
        exit 4
    fi
}

apply_gio_unix_backports() {
    if [[ ! -f "${GIO_UNIX_PATCH_TARGET}" ]]; then
        echo "WPE backport target is missing: ${GIO_UNIX_PATCH_TARGET}" >&2
        exit 4
    fi

    apply_backport "${GIO_UNIX_PATCH}" "GioUnix dependency and includes"
    apply_backport "${LIBSECCOMP_SCOPED_LINK_PATCH}" "libseccomp scoped link"
    apply_backport "${LIBSECCOMP_POST_TARGET_PATCH}" "libseccomp post-target link"
}

if [[ "${WPE_VERSION}" != "${EXPECTED_VERSION}" ]]; then
    echo "Unsupported WPE_VERSION=${WPE_VERSION}; update the pinned checksum before changing versions." >&2
    exit 2
fi

mkdir -p "${DOWNLOAD_DIR}" "${SOURCE_DIR}"

if [[ -f "${VERSION_FILE}" ]] && [[ "$(<"${VERSION_FILE}")" == "${WPE_VERSION}" ]]; then
    echo "WPE WebKit ${WPE_VERSION} source is already available at ${SOURCE_DIR}."
else
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
fi

apply_gio_unix_backports

echo "Prepared WPE WebKit ${WPE_VERSION} source at ${SOURCE_DIR}."
