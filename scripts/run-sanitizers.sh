#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "${project_root}"
cmake --preset asan
cmake --build --preset asan -j2
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1:strict_string_checks=1} \
UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1} \
ctest --preset asan --output-on-failure

