#!/usr/bin/env bash
# JingWei
# tools test runtime-report.sh    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly REPOSITORY_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
readonly COMPOSE_FILE="${REPOSITORY_DIR}/compose.yaml"

compose=(
    docker compose
    --project-directory "${REPOSITORY_DIR}"
    --file "${COMPOSE_FILE}"
)

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    echo "Usage: tools/test/runtime-report.sh"
    exit 0
fi
if [[ $# -ne 0 ]]; then
    echo "Usage: tools/test/runtime-report.sh" >&2
    exit 2
fi
if ! command -v docker >/dev/null 2>&1 ||
    ! command -v python3 >/dev/null 2>&1; then
    echo "docker and python3 are required." >&2
    exit 127
fi

container_id="$("${compose[@]}" ps --quiet jw-browser)"
if [[ -z "${container_id}" ]]; then
    echo "jw-browser is not running; run tools/test/smoke-browser.sh first." >&2
    exit 69
fi

started_at="$(docker inspect --format '{{.State.StartedAt}}' "${container_id}")"
health="$(docker inspect --format \
    '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' \
    "${container_id}")"
image_name="$(docker inspect --format '{{.Config.Image}}' "${container_id}")"
ready_line="$(docker logs --timestamps "${container_id}" 2>&1 |
    awk '/ JW_WPE_READY / && !found { print; found = 1 }')"
if [[ -z "${ready_line}" ]]; then
    echo "jw-browser has not emitted JW_WPE_READY." >&2
    exit 75
fi

ready_at="${ready_line%% *}"
ready_marker="${ready_line#* }"
ready_ms="$(python3 -c '
from datetime import datetime
import re
import sys

def parse(value: str) -> datetime:
    normalized = value.replace("Z", "+00:00")
    normalized = re.sub(r"(\.\d{6})\d+(?=[+-])", r"\1", normalized)
    return datetime.fromisoformat(normalized)

elapsed = (parse(sys.argv[2]) - parse(sys.argv[1])).total_seconds() * 1000
if elapsed < 0:
    raise SystemExit("READY timestamp precedes container start")
print(f"{elapsed:.3f}")
' "${started_at}" "${ready_at}")"

echo "RUNTIME container=${container_id} health=${health}"
echo "RUNTIME endpoint=$("${compose[@]}" port jw-browser "${VNC_PORT:-5900}")"
echo "RUNTIME ready_ms=${ready_ms} ${ready_marker}"
docker stats --no-stream \
    --format 'RUNTIME cgroup_cpu={{.CPUPerc}} cgroup_memory={{.MemUsage}}' \
    "${container_id}"
docker image inspect "${image_name}" \
    --format 'RUNTIME development_image_bytes={{.Size}}'

"${compose[@]}" exec --no-TTY jw-browser sh -lc '
    prefix_bytes=$(du -sb "${WPE_PREFIX_DIR}" | cut -f1)
    printf "RUNTIME wpe_prefix_bytes=%s\n" "${prefix_bytes}"
    stat -c "RUNTIME wpe_library_bytes=%s" \
        "${WPE_PREFIX_DIR}/lib/libWPEWebKit-2.0.so.1.9.9"
    stat -c "RUNTIME browser_bytes=%s" "${JINGWEI_BROWSER_BIN}"
    printf "RUNTIME JSC_useJIT_requested=%s cache_model=%s\n" \
        "${JSC_useJIT:-<unset>}" "${JINGWEI_CACHE_MODEL:-<unset>}"
    python3 -c '\''
import psutil

names = {"jw-wpe-browser", "WPEWebProcess", "WPENetworkProcess"}
rows = []
for process in psutil.process_iter(["pid", "name"]):
    if process.info["name"] not in names:
        continue
    try:
        memory = process.memory_full_info()
        rows.append((
            process.info["pid"],
            process.info["name"],
            memory.rss,
            getattr(memory, "pss", 0),
        ))
    except (psutil.AccessDenied, psutil.NoSuchProcess):
        pass
for pid, name, rss, pss in sorted(rows):
    print(f"RUNTIME process={name} pid={pid} rss_bytes={rss} pss_bytes={pss}")
'\''
'
