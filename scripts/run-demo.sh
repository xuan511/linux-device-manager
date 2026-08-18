#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
binary_dir=${DEVMGR_BINARY_DIR:-"${project_root}/build/debug"}
work_dir=$(mktemp -d)
sim_pid=
daemon_pid=

cleanup() {
    if [[ -n ${daemon_pid} ]]; then kill -TERM "${daemon_pid}" 2>/dev/null || true; wait "${daemon_pid}" 2>/dev/null || true; fi
    if [[ -n ${sim_pid} ]]; then kill -TERM "${sim_pid}" 2>/dev/null || true; wait "${sim_pid}" 2>/dev/null || true; fi
    rm -rf -- "${work_dir}"
}
trap cleanup EXIT INT TERM

if [[ ! -x "${binary_dir}/device-sim" ]]; then
    cmake --preset debug
    cmake --build --preset debug -j2
fi

socket_path="${work_dir}/devmgrd.sock"
firmware_path="${work_dir}/firmware.bin"
dd if=/dev/zero of="${firmware_path}" bs=1024 count=8 status=none

echo "[1/8] Starting device simulator"
"${binary_dir}/device-sim" >"${work_dir}/sim.out" 2>"${work_dir}/sim.err" &
sim_pid=$!
for _ in $(seq 1 100); do
    grep -q '^PTY: ' "${work_dir}/sim.out" && break
    kill -0 "${sim_pid}" 2>/dev/null || { cat "${work_dir}/sim.err" >&2; exit 1; }
    sleep 0.05
done
pty_path=$(sed -n 's/^PTY: //p' "${work_dir}/sim.out" | head -n1)
[[ -n ${pty_path} ]] || { echo "simulator did not publish PTY" >&2; exit 1; }
echo "PTY: ${pty_path}"

echo "[2/8] Starting devmgrd"
"${binary_dir}/devmgrd" --device "${pty_path}" --socket "${socket_path}" \
    >"${work_dir}/daemon.out" 2>"${work_dir}/daemon.err" &
daemon_pid=$!
for _ in $(seq 1 100); do
    [[ -S ${socket_path} ]] && "${binary_dir}/devctl" --socket "${socket_path}" ping >/dev/null 2>&1 && break
    kill -0 "${daemon_pid}" 2>/dev/null || { cat "${work_dir}/daemon.err" >&2; exit 1; }
    sleep 0.05
done

echo "[3/8] Device information"
"${binary_dir}/devctl" --socket "${socket_path}" info
echo "[4/8] Ping and health"
"${binary_dir}/devctl" --socket "${socket_path}" ping
"${binary_dir}/devctl" --socket "${socket_path}" health
echo "[5/8] Telemetry"
"${binary_dir}/devctl" --socket "${socket_path}" telemetry-start 100
sleep 0.2
"${binary_dir}/devctl" --socket "${socket_path}" telemetry
"${binary_dir}/devctl" --socket "${socket_path}" telemetry-stop
echo "[6/8] Firmware upgrade"
"${binary_dir}/devctl" --socket "${socket_path}" upgrade "${firmware_path}" 1.1.0
echo "[7/8] Confirming version and statistics"
info_output=$("${binary_dir}/devctl" --socket "${socket_path}" info)
printf '%s\n' "${info_output}"
grep -q '^Firmware: 1.1.0$' <<<"${info_output}"
"${binary_dir}/devctl" --socket "${socket_path}" stats
echo "[8/8] Clean shutdown"
kill -TERM "${daemon_pid}"
wait "${daemon_pid}"
daemon_pid=
[[ ! -e ${socket_path} ]]
echo "PASS"

