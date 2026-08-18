#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
binary_dir=${DEVMGR_BINARY_DIR:-"${project_root}/build/debug"}
socket_path=${DEVMGR_SOCKET:-"/tmp/devmgrd-$(id -u).sock"}
iterations=${1:-100}
[[ ${iterations} =~ ^[1-9][0-9]*$ ]] || { echo "iterations must be positive" >&2; exit 2; }
start_ns=$(date +%s%N)
for _ in $(seq 1 "${iterations}"); do
    "${binary_dir}/devctl" --socket "${socket_path}" ping >/dev/null
done
end_ns=$(date +%s%N)
elapsed_ns=$((end_ns - start_ns))
echo "requests: ${iterations}"
echo "total_ms: $((elapsed_ns / 1000000))"
awk -v ns="${elapsed_ns}" -v n="${iterations}" 'BEGIN { printf "average_ms: %.3f\nrequests_per_sec: %.2f\n", ns/n/1000000, n*1000000000/ns }'

