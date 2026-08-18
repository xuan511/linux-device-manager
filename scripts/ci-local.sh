#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "${project_root}"
cmake --preset debug
cmake --build --preset debug -j2
ctest --preset debug --output-on-failure
cmake --preset release
cmake --build --preset release -j2
"${project_root}/scripts/run-sanitizers.sh"

