#!/usr/bin/env bash
# JingWei
# tools dev rebuild-jingwei.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly REPOSITORY_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
readonly COMPOSE_FILE="${REPOSITORY_DIR}/compose.yaml"
readonly WAIT_TIMEOUT="${JINGWEI_WAIT_TIMEOUT:-900}"
readonly WAIT_INTERVAL="${JINGWEI_WAIT_INTERVAL:-5}"

compose=(
    docker compose
    --project-directory "${REPOSITORY_DIR}"
    --file "${COMPOSE_FILE}"
)

usage() {
    cat <<'USAGE'
Usage: tools/dev/rebuild-jingwei.sh

Refresh the Docker source snapshot, then rebuild only JingWei and its browser
adapter in the persistent jingwei-build volume. This command never invokes the
WPE/WebKit build scripts and never removes Compose volumes.

Environment:
  JINGWEI_WAIT_TIMEOUT   Seconds to wait for the WPE prefix (default: 900)
  JINGWEI_WAIT_INTERVAL  Poll interval in seconds (default: 5)
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if [[ $# -ne 0 ]]; then
    usage >&2
    exit 2
fi

if ! command -v docker >/dev/null 2>&1; then
    echo "docker is required." >&2
    exit 127
fi

if ! [[ "${WAIT_TIMEOUT}" =~ ^[0-9]+$ && "${WAIT_INTERVAL}" =~ ^[1-9][0-9]*$ ]]; then
    echo "JINGWEI_WAIT_TIMEOUT must be a non-negative integer and JINGWEI_WAIT_INTERVAL must be positive." >&2
    exit 2
fi

echo "Refreshing the cached JingWei source snapshot (no host bind mount)."
"${compose[@]}" build wpe-dev

echo "Waiting for the existing WPE prefix, then rebuilding JingWei in its persistent build volume."
"${compose[@]}" run --rm --no-deps --no-TTY \
    --entrypoint /bin/bash \
    -e "JINGWEI_WAIT_TIMEOUT=${WAIT_TIMEOUT}" \
    -e "JINGWEI_WAIT_INTERVAL=${WAIT_INTERVAL}" \
    wpe-dev \
    -euo pipefail -c '
        deadline=$((SECONDS + JINGWEI_WAIT_TIMEOUT))
        while [[ ! -d "${WPE_PREFIX_DIR}/include" ]] \
            || ! find "${WPE_PREFIX_DIR}/lib" -name wpe-webkit-2.0.pc -print -quit 2>/dev/null | grep -q .; do
            if (( SECONDS >= deadline )); then
                echo "WPE prefix is not ready at ${WPE_PREFIX_DIR} (wpe-webkit-2.0.pc is missing); the WPE build may still be installing." >&2
                exit 75
            fi
            echo "WPE prefix is not ready at ${WPE_PREFIX_DIR}; waiting ${JINGWEI_WAIT_INTERVAL}s..."
            sleep "${JINGWEI_WAIT_INTERVAL}"
        done
        pkgconfig_file="$(find "${WPE_PREFIX_DIR}/lib" -name wpe-webkit-2.0.pc -print -quit)"
        export PKG_CONFIG_PATH="$(dirname "${pkgconfig_file}"):${PKG_CONFIG_PATH:-}"
        exec /workspace/JingWei/tools/container/build-jingwei.sh
    '

if [[ -n "$("${compose[@]}" ps --status running --quiet jw-browser)" ]]; then
    echo "The browser was running; recreating it with the new source snapshot and binary."
    "${compose[@]}" up --detach --no-build --force-recreate jw-browser
fi

echo "JingWei rebuild completed in wpe-dev. WPE build and prefix volumes were reused unchanged."
