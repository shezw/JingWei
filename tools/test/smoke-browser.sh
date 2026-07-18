#!/usr/bin/env bash
# JingWei
# tools test smoke-browser.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly REPOSITORY_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
readonly COMPOSE_FILE="${REPOSITORY_DIR}/compose.yaml"
readonly WAIT_TIMEOUT="${SMOKE_WAIT_TIMEOUT:-30}"
readonly WAIT_INTERVAL="${SMOKE_WAIT_INTERVAL:-2}"
readonly RFB_TIMEOUT="${SMOKE_RFB_TIMEOUT:-10}"
readonly COMMAND_TIMEOUT="${SMOKE_COMMAND_TIMEOUT:-60}"
readonly E2E_TIMEOUT="${SMOKE_E2E_TIMEOUT:-60}"
readonly HOST_PORT="${JINGWEI_VNC_HOST_PORT:-5900}"
readonly OUTPUT_BASE_DIR="${SMOKE_OUTPUT_DIR:-${REPOSITORY_DIR}/test-results/rfb-wpe-e2e}"

compose=(
    docker compose
    --project-directory "${REPOSITORY_DIR}"
    --file "${COMPOSE_FILE}"
)

usage() {
    cat <<'USAGE'
Usage: tools/test/smoke-browser.sh

Rebuild JingWei from the current worktree, then force-recreate jw-browser with
a unique fixture token. The smoke gate verifies:
  - the container becomes healthy;
  - Docker publishes VNC only on the configured Mac loopback port;
  - the new container emitted JW_WPE_READY for the tokenized fixture URL;
  - a host-side RFB 3.8 client captures the real 1280x720 framebuffer;
  - RFB pointer and exact-token keyboard input produce deterministic changes.

Environment:
  SMOKE_WAIT_TIMEOUT   Seconds to wait (default: 30)
  SMOKE_WAIT_INTERVAL  Poll interval in seconds (default: 2)
  SMOKE_RFB_TIMEOUT    Seconds per RFB/state operation (default: 10)
  SMOKE_COMMAND_TIMEOUT  Seconds per Docker command (default: 60)
  SMOKE_E2E_TIMEOUT    Overall Python client timeout (default: 60)
  SMOKE_OUTPUT_DIR     Parent directory for unique per-token PPM evidence
  JINGWEI_VNC_HOST_PORT  Mac loopback port (default: 5900)
USAGE
}

run_with_timeout() {
    local timeout_seconds="$1"
    shift
    python3 -c '
import os
import signal
import subprocess
import sys

timeout = float(sys.argv[1])
command = sys.argv[2:]
process = subprocess.Popen(command, start_new_session=True)
try:
    return_code = process.wait(timeout=timeout)
except subprocess.TimeoutExpired:
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=1)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
    print(
        f"Timed out after {timeout:g}s: {command[0]}",
        file=sys.stderr,
    )
    sys.exit(124)
sys.exit(return_code)
' "${timeout_seconds}" "$@"
}

has_exact_ready_line() {
    local logs="$1"
    local expected_url="$2"
    local expected_port="$3"
    local ready_prefix="JW_WPE_READY url=${expected_url} size=1280x720 vnc_port=${expected_port} frames="
    local log_line=""
    local frame_count=""

    while IFS= read -r log_line; do
        if [[ "${log_line:0:${#ready_prefix}}" != "${ready_prefix}" ]]; then
            continue
        fi
        frame_count="${log_line:${#ready_prefix}}"
        if [[ "${frame_count}" =~ ^[1-9][0-9]*$ ]]; then
            return 0
        fi
    done <<<"${logs}"
    return 1
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if [[ $# -ne 0 ]]; then
    usage >&2
    exit 2
fi

if ! [[ "${WAIT_TIMEOUT}" =~ ^[0-9]+$ \
    && "${WAIT_INTERVAL}" =~ ^[1-9][0-9]*$ \
    && "${RFB_TIMEOUT}" =~ ^[1-9][0-9]*$ \
    && "${COMMAND_TIMEOUT}" =~ ^[1-9][0-9]*$ \
    && "${E2E_TIMEOUT}" =~ ^[1-9][0-9]*$ \
    && "${HOST_PORT}" =~ ^[1-9][0-9]*$ ]] \
    || (( HOST_PORT > 65535 )); then
    echo "Smoke timeouts and JINGWEI_VNC_HOST_PORT must be valid positive integers." >&2
    exit 2
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 is required for the host-side RFB end-to-end test." >&2
    exit 127
fi

if ! command -v docker >/dev/null 2>&1; then
    echo "docker is required for the browser smoke test." >&2
    exit 127
fi

echo "Rebuilding JingWei from the current worktree before the smoke gate."
"${REPOSITORY_DIR}/tools/dev/rebuild-jingwei.sh"

token="$(python3 -c 'import secrets; print("jw" + secrets.token_hex(8))')"
browser_url="file:///workspace/JingWei/tests/fixtures/wpe-vnc-smoke.html?token=${token}"
run_output_dir="${OUTPUT_BASE_DIR}/${token}"
mkdir -p "${run_output_dir}"
rm -f -- \
    "${run_output_dir}/initial.ppm" \
    "${run_output_dir}/clicked.ppm" \
    "${run_output_dir}/typed.ppm"

echo "Force-recreating jw-browser with deterministic fixture token ${token}."
run_with_timeout "${COMMAND_TIMEOUT}" env \
    "JINGWEI_BROWSER_URL=${browser_url}" \
    "JINGWEI_VNC_HOST_PORT=${HOST_PORT}" \
    "${compose[@]}" up --detach --no-build --force-recreate jw-browser

container_id="$(run_with_timeout \
    "${COMMAND_TIMEOUT}" "${compose[@]}" ps --quiet jw-browser)"
if [[ -z "${container_id}" ]]; then
    echo "jw-browser did not start after force-recreate." >&2
    exit 69
fi

deadline=$((SECONDS + WAIT_TIMEOUT))
browser_logs=""
while true; do
    current_container_id="$(run_with_timeout \
        "${COMMAND_TIMEOUT}" "${compose[@]}" ps --quiet jw-browser)"
    if [[ "${current_container_id}" != "${container_id}" ]]; then
        echo "jw-browser container changed during readiness: ${container_id} -> ${current_container_id:-<none>}" >&2
        exit 1
    fi
    health="$(run_with_timeout "${COMMAND_TIMEOUT}" docker inspect \
        --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' \
        "${container_id}")"
    if [[ "${health}" == "healthy" ]]; then
        browser_logs="$(run_with_timeout \
            "${COMMAND_TIMEOUT}" docker logs "${container_id}" 2>&1)"
        if has_exact_ready_line \
            "${browser_logs}" "${browser_url}" "${VNC_PORT:-5900}"; then
            break
        fi
    fi
    if [[ "${health}" == "exited" || "${health}" == "dead" || "${health}" == "unhealthy" ]]; then
        echo "jw-browser entered terminal state: ${health}" >&2
        exit 1
    fi
    if (( SECONDS >= deadline )); then
        echo "Timed out waiting for jw-browser health; last state: ${health}" >&2
        exit 75
    fi
    sleep "${WAIT_INTERVAL}"
done

published_endpoint="$(run_with_timeout \
    "${COMMAND_TIMEOUT}" "${compose[@]}" port jw-browser "${VNC_PORT:-5900}")"
if [[ "${published_endpoint}" != "127.0.0.1:${HOST_PORT}" ]]; then
    echo "Unsafe or missing VNC publication: ${published_endpoint:-<none>}" >&2
    exit 1
fi

run_with_timeout "${E2E_TIMEOUT}" python3 "${SCRIPT_DIR}/rfb-wpe-e2e.py" \
    --token "${token}" \
    --port "${HOST_PORT}" \
    --timeout "${RFB_TIMEOUT}" \
    --output-dir "${run_output_dir}"

sleep 1
post_e2e_container_id="$(run_with_timeout \
    "${COMMAND_TIMEOUT}" "${compose[@]}" ps --quiet jw-browser)"
if [[ "${post_e2e_container_id}" != "${container_id}" ]]; then
    echo "jw-browser container changed after RFB E2E: ${container_id} -> ${post_e2e_container_id:-<none>}" >&2
    exit 1
fi
post_e2e_health="$(run_with_timeout "${COMMAND_TIMEOUT}" docker inspect \
    --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' \
    "${container_id}")"
if [[ "${post_e2e_health}" != "healthy" ]]; then
    echo "jw-browser is not healthy after RFB E2E: ${post_e2e_health}" >&2
    exit 1
fi

echo "true: token=${token} container=${container_id} post_e2e_health=${post_e2e_health} evidence=${run_output_dir}"
