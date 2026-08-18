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

socket_path="${work_dir}/devmgrd.sock"
firmware_path="${work_dir}/firmware.bin"
dd if=/dev/zero of="${firmware_path}" bs=1024 count=16 status=none
"${binary_dir}/device-sim" --disconnect-at-percent 42 --seed 7 >"${work_dir}/sim.out" 2>"${work_dir}/sim.err" &
sim_pid=$!
for _ in $(seq 1 100); do grep -q '^PTY: ' "${work_dir}/sim.out" && break; sleep 0.05; done
pty_path=$(sed -n 's/^PTY: //p' "${work_dir}/sim.out" | head -n1)
[[ -n ${pty_path} ]]
"${binary_dir}/devmgrd" --device "${pty_path}" --socket "${socket_path}" >"${work_dir}/daemon.out" 2>"${work_dir}/daemon.err" &
daemon_pid=$!
ready=0
for _ in $(seq 1 100); do
    if [[ -S ${socket_path} ]] && "${binary_dir}/devctl" --socket "${socket_path}" ping >/dev/null 2>&1; then ready=1; break; fi
    kill -0 "${daemon_pid}" 2>/dev/null || { cat "${work_dir}/daemon.err" >&2; exit 1; }
    kill -0 "${sim_pid}" 2>/dev/null || { cat "${work_dir}/sim.err" >&2; exit 1; }
    sleep 0.05
done
if (( ready == 0 )); then
    echo "daemon did not reach READY" >&2
    cat "${work_dir}/daemon.err" >&2
    exit 1
fi
echo "Upgrading with a response blackout at 42% (expect FW_STATUS resume)"
"${binary_dir}/devctl" --socket "${socket_path}" upgrade "${firmware_path}" 1.2.0
info_output=$("${binary_dir}/devctl" --socket "${socket_path}" info)
printf '%s\n' "${info_output}"
grep -q '^Firmware: 1.2.0$' <<<"${info_output}"
echo "FAULT RECOVERY PASS"
