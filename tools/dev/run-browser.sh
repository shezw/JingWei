#!/usr/bin/env bash
# JingWei
# tools dev run-browser.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly DEFAULT_BROWSER_BIN="/workspace/jingwei-build/apps/jw-wpe-browser/jw-wpe-browser"
readonly DEFAULT_WAIT_TIMEOUT=900
readonly DEFAULT_WAIT_INTERVAL=5

usage() {
    cat <<'USAGE'
Usage: tools/dev/run-browser.sh [--detach] [URL]
       tools/dev/run-browser.sh --container

Start the jw-browser Compose service without rebuilding its image or any WPE
artifacts. The VNC endpoint is available at vnc://127.0.0.1:5900 by default.

Environment:
  JINGWEI_BROWSER_URL       URL used when no URL argument is supplied
  JINGWEI_VNC_HOST_PORT     Mac loopback port (default: 5900)
  JINGWEI_WAIT_TIMEOUT      Readiness timeout in seconds (default: 900)
  JINGWEI_WAIT_INTERVAL     Container polling interval (default: 5)
  VNC_PORT                  Container VNC port (default: 5900)
USAGE
}

wait_for_file() {
    local target="$1"
    local description="$2"
    local timeout="$3"
    local interval="$4"
    local deadline=$((SECONDS + timeout))

    until [[ -e "${target}" ]]; do
        if (( SECONDS >= deadline )); then
            echo "Timed out waiting for ${description}: ${target}" >&2
            return 75
        fi
        echo "Waiting for ${description}: ${target} (${interval}s poll interval)"
        sleep "${interval}"
    done
}

wait_for_wpe_prefix() {
    local prefix_dir="$1"
    local timeout="$2"
    local interval="$3"
    local deadline=$((SECONDS + timeout))
    local pkgconfig_file=""

    while true; do
        if [[ -d "${prefix_dir}/include" && -d "${prefix_dir}/lib" ]]; then
            pkgconfig_file="$(find "${prefix_dir}/lib" -name wpe-webkit-2.0.pc -print -quit 2>/dev/null)"
            if [[ -n "${pkgconfig_file}" ]]; then
                printf '%s\n' "${pkgconfig_file}"
                return 0
            fi
        fi
        if (( SECONDS >= deadline )); then
            echo "Timed out waiting for the installed WPE prefix: ${prefix_dir}" >&2
            return 75
        fi
        echo "Waiting for the installed WPE prefix: ${prefix_dir} (${interval}s poll interval)" >&2
        sleep "${interval}"
    done
}

run_in_container() {
    local browser_bin="${JINGWEI_BROWSER_BIN:-${DEFAULT_BROWSER_BIN}}"
    local browser_url="${JINGWEI_BROWSER_URL:-about:blank}"
    local prefix_dir="${WPE_PREFIX_DIR:-/workspace/wpe/prefix/${WPE_VERSION:-2.52.5}-${WPE_PROFILE:-container}}"
    local timeout="${JINGWEI_WAIT_TIMEOUT:-${DEFAULT_WAIT_TIMEOUT}}"
    local interval="${JINGWEI_WAIT_INTERVAL:-${DEFAULT_WAIT_INTERVAL}}"
    local pkgconfig_file=""
    local runtime_library=""
    local runtime_dir="${XDG_RUNTIME_DIR:-/tmp/runtime-jw}"
    local jit_setting="${JSC_useJIT:-false}"

    if ! [[ "${timeout}" =~ ^[0-9]+$ && "${interval}" =~ ^[1-9][0-9]*$ ]]; then
        echo "JINGWEI_WAIT_TIMEOUT must be non-negative and JINGWEI_WAIT_INTERVAL must be positive." >&2
        exit 2
    fi
    case "${jit_setting}" in
        true|false)
            ;;
        *)
            echo "JSC_useJIT must be true or false, received: ${jit_setting}" >&2
            exit 2
            ;;
    esac

    pkgconfig_file="$(wait_for_wpe_prefix "${prefix_dir}" "${timeout}" "${interval}")"
    wait_for_file "${browser_bin}" "jw-wpe-browser binary" "${timeout}" "${interval}"

    if [[ ! -x "${browser_bin}" ]]; then
        echo "Browser exists but is not executable: ${browser_bin}" >&2
        exit 126
    fi

    runtime_library="$(find "${prefix_dir}/lib" -name 'libWPEWebKit-2.0.so*' -print -quit 2>/dev/null)"
    if [[ -z "${runtime_library}" ]]; then
        echo "WPE runtime library is missing below ${prefix_dir}/lib." >&2
        exit 75
    fi

    export PKG_CONFIG_PATH="$(dirname "${pkgconfig_file}"):${PKG_CONFIG_PATH:-}"
    export LD_LIBRARY_PATH="$(dirname "${runtime_library}"):${LD_LIBRARY_PATH:-}"
    export XDG_RUNTIME_DIR="${runtime_dir}"
    export JSC_useJIT="${jit_setting}"
    mkdir -p "${runtime_dir}"
    chmod 0700 "${runtime_dir}"

    exec "${browser_bin}" "${browser_url}"
}

if [[ "${1:-}" == "--container" ]]; then
    shift
    if [[ $# -ne 0 ]]; then
        usage >&2
        exit 2
    fi
    run_in_container
fi

detach=false
if [[ "${1:-}" == "--detach" ]]; then
    detach=true
    shift
fi

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if [[ $# -gt 1 ]]; then
    usage >&2
    exit 2
fi

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly REPOSITORY_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
readonly COMPOSE_FILE="${REPOSITORY_DIR}/compose.yaml"
readonly BROWSER_URL="${1:-${JINGWEI_BROWSER_URL:-about:blank}}"
readonly HOST_PORT="${JINGWEI_VNC_HOST_PORT:-5900}"
readonly WAIT_TIMEOUT="${JINGWEI_WAIT_TIMEOUT:-${DEFAULT_WAIT_TIMEOUT}}"

compose=(
    docker compose
    --project-directory "${REPOSITORY_DIR}"
    --file "${COMPOSE_FILE}"
)

if ! command -v docker >/dev/null 2>&1; then
    echo "docker is required." >&2
    exit 127
fi

if ! [[ "${WAIT_TIMEOUT}" =~ ^[0-9]+$ && "${HOST_PORT}" =~ ^[1-9][0-9]*$ ]] \
    || (( HOST_PORT > 65535 )); then
    echo "JINGWEI_WAIT_TIMEOUT must be non-negative and JINGWEI_VNC_HOST_PORT must be in 1..65535." >&2
    exit 2
fi

echo "Starting jw-browser from the existing image and persistent build volumes."
JINGWEI_BROWSER_URL="${BROWSER_URL}" \
JINGWEI_WAIT_TIMEOUT="${WAIT_TIMEOUT}" \
JINGWEI_VNC_HOST_PORT="${HOST_PORT}" \
    "${compose[@]}" up --detach --no-build --force-recreate jw-browser

echo "Waiting for the VNC endpoint and browser readiness (timeout: ${WAIT_TIMEOUT}s)."
deadline=$((SECONDS + WAIT_TIMEOUT))
browser_logs=""
while true; do
    container_id="$("${compose[@]}" ps --quiet jw-browser)"
    if [[ -n "${container_id}" ]]; then
        health="$(docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' "${container_id}")"
        case "${health}" in
            healthy)
                browser_logs="$("${compose[@]}" logs --no-color --no-log-prefix jw-browser)"
                if grep -q '^JW_WPE_READY ' <<<"${browser_logs}"; then
                    break
                fi
                ;;
            exited|dead|unhealthy)
                "${compose[@]}" logs --no-color --tail 80 jw-browser >&2
                echo "jw-browser failed readiness with state: ${health}" >&2
                exit 1
                ;;
        esac
    fi
    if (( SECONDS >= deadline )); then
        "${compose[@]}" logs --no-color --tail 80 jw-browser >&2
        echo "Timed out waiting for jw-browser readiness." >&2
        exit 75
    fi
    sleep 2
done

echo "jw-browser is ready at vnc://127.0.0.1:${HOST_PORT} (URL: ${BROWSER_URL})."
if [[ "${detach}" == false ]]; then
    echo "Following logs; press Ctrl-C to stop following (the container remains running)."
    "${compose[@]}" logs --follow --no-color jw-browser
fi
