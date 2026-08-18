#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "${project_root}"
cmake --preset coverage
cmake --build --preset coverage -j2
ctest --preset coverage --output-on-failure
if command -v lcov >/dev/null && command -v genhtml >/dev/null; then
    lcov --capture --directory build/coverage --output-file build/coverage/coverage.info \
        --ignore-errors mismatch
    lcov --remove build/coverage/coverage.info '/usr/*' '*/tests/*' \
        --output-file build/coverage/coverage.info
    genhtml build/coverage/coverage.info --output-directory build/coverage/html
    echo "Coverage report: build/coverage/html/index.html"
else
    echo "lcov/genhtml not installed; .gcda/.gcno files are available under build/coverage"
fi

