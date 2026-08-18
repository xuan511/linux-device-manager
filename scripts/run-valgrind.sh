#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
binary_dir=${DEVMGR_BINARY_DIR:-"${project_root}/build/debug"}
command -v valgrind >/dev/null || { echo "valgrind is required" >&2; exit 2; }
cmake --preset debug
cmake --build --preset debug -j2

tests=(test_common test_protocol test_session test_ipc test_upgrade test_parser_stress
       test_transport test_worker)
for test_name in "${tests[@]}"; do
    echo "Valgrind: ${test_name}"
    valgrind --quiet --leak-check=full --show-leak-kinds=definite \
        --errors-for-leak-kinds=definite --error-exitcode=99 \
        "${binary_dir}/tests/${test_name}"
done

echo "Valgrind: simulator/daemon/CLI demo"
DEVMGR_BINARY_DIR="${binary_dir}" DEVMGR_VALGRIND=1 "${project_root}/scripts/run-demo.sh"
