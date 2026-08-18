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

Tests intentionally verify happy paths, malformed boundaries, corruption,
timeouts, duplicate data, and recovery. They do not claim physical UART/STM32
timing or flash power-loss behavior.

