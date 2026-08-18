# Testing Strategy

- Unit tests validate CRC vectors, ring boundaries/wrap, frame codec/parser
  recovery, IPC bounds, state transitions, retry deadlines, transport PTY I/O,
  upgrade progress, invalid offsets, and FW_STATUS resume.
- `stress.parser` feeds 2000 valid frames through pseudo-random 1–23 byte slices
  with garbage inserted, then runs 10,000 random-input iterations.
- `integration.demo` starts all three processes and verifies query, telemetry,
  upgrade, version confirmation, statistics, SIGTERM, and socket cleanup.
- `integration.fault-recovery` creates a deterministic six-second blackout at
  42 percent and requires successful version confirmation afterward.
- `tests/fuzz/fuzz_parser.c` is a libFuzzer entry point for Clang campaigns; the
  deterministic smoke loop runs everywhere without special tooling.

Run everything with `ctest --preset debug --output-on-failure`. Select with
`ctest --test-dir build/debug -L integration` or `-L stress`. Sanitizer presets
run the same suite, so process-level paths are checked as well as unit tests.
Valgrind and coverage commands are documented and scripted in their phases.

`scripts/run-sanitizers.sh` configures the combined ASan+UBSan preset and runs
the full suite with leak detection and halt-on-first-error. `scripts/run-valgrind.sh`
runs every host test plus the real three-process demo with definite leaks and
invalid accesses treated as failures. `scripts/run-coverage.sh` runs the full
coverage build and emits lcov HTML when those optional tools are installed.
GitHub Actions runs normal Ubuntu 24.04, ASan+UBSan Ubuntu 24.04, and Valgrind
Ubuntu 22.04 jobs independently.

Tests intentionally verify happy paths, malformed boundaries, corruption,
timeouts, duplicate data, and recovery. They do not claim physical UART/STM32
timing or flash power-loss behavior.
