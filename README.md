# Linux Embedded Device Manager & Firmware Update Framework

[![CI](https://github.com/xuan511/linux-device-manager/actions/workflows/ci.yml/badge.svg)](https://github.com/xuan511/linux-device-manager/actions/workflows/ci.yml)

A C17 Linux systems project that manages a serial embedded device through an
epoll daemon, a bounded Unix-socket API, and a binary protocol with resumable,
idempotent firmware updates. A PTY simulator makes the complete host path
repeatable without root or physical hardware.

[中文说明](README.zh-CN.md) · [Architecture](docs/architecture.md) ·
[Wire protocol](docs/protocol.md) · [Learning guide](docs/learning-guide.zh-CN.md)

## Why It Exists

Firmware experience often stops at UART/IAP code, while Linux device software
also needs stream parsing, fd ownership, event-driven concurrency, IPC,
timeouts, process lifecycle, observability, and failure recovery. This project
joins those concerns in one inspectable reference implementation without a web
stack or framework hiding the system calls.

## Architecture

```text
devctl ── length-bounded AF_UNIX IPC ── devmgrd (single epoll reactor)
                                             │
                          session / timeout / upgrade state machines
                                             │
                           binary codec + CRC32 stream parser
                                             │
                              termios serial / PTY transport
                                             │
                                          device-sim
```

The daemon owns every fd and mutable session object. `timerfd` drives monotonic
deadlines, `signalfd` orders shutdown, and fixed output buffers preserve partial
writes. Firmware upgrade is a daemon-owned operation: the start IPC returns an
ID, while the CLI polls progress over short connections. Closing the CLI does
not cancel a safe device operation.

## Key Features

- Explicit little-endian versioned frames; CRC32/ISO-HDLC known vectors
- Ring-buffer stream parser for fragments, coalesced frames, garbage, bad CRC,
  invalid lengths, and resynchronization
- Nonblocking 115200 8N1 raw termios transport and direct POSIX PTY creation
- epoll + timerfd + signalfd reactor; no busy loop or thread-per-device model
- Sequence correlation, duplicate/stale accounting, bounded retries
- PING, info, health, statistics, and asynchronous telemetry
- 1024-byte firmware chunks, mmap validation, verify/activate/reboot
- Lost-ACK idempotency and bounded FW_STATUS resume after a response blackout
- Drop, corruption, delay, blackout-at-percentage, and verify-failure injection
- Unit, stress, random-input, PTY, full-process, sanitizer, and Valgrind tests
- Cross-compiled Cortex-M3 portable protocol/update reference core

## Quick Demo

Requirements: Ubuntu 22.04/24.04 or WSL2 Ubuntu, GCC, CMake 3.20+, Bash.

```sh
./scripts/run-demo.sh
./scripts/demo-fault-recovery.sh
```

The first command builds if needed, creates a PTY, starts all three processes,
queries the simulated device, streams telemetry, upgrades an 8 KiB image,
confirms version `1.1.0`, sends SIGTERM, and verifies socket cleanup. The fault
demo creates a six-second response blackout at 42%, exhausts ordinary data
retries, queries FW_STATUS, resumes, and confirms version `1.2.0`.

## Build

```sh
cmake --preset debug
cmake --build --preset debug -j
cmake --preset release
cmake --build --preset release -j
cmake --install build/release --prefix /tmp/devmgr-install
```

The top-level Makefile only delegates: `make build`, `make test`, `make demo`,
and `make clean`.

## Testing

```sh
ctest --preset debug --output-on-failure
./scripts/run-sanitizers.sh
./scripts/run-valgrind.sh
./scripts/run-coverage.sh
```

GitHub Actions runs Ubuntu 24.04 debug/integration/release/install,
ASan+UBSan, Ubuntu 22.04 Valgrind including the process demo, and an
`arm-none-eabi-gcc` Cortex-M3 cross-compile. See [testing.md](docs/testing.md).

## Manual Run

```sh
# terminal 1; copy the printed /dev/pts/N path
build/debug/device-sim

# terminal 2
build/debug/devmgrd --device /dev/pts/N --socket /tmp/devmgrd.sock

# terminal 3
build/debug/devctl --socket /tmp/devmgrd.sock info
build/debug/devctl --socket /tmp/devmgrd.sock telemetry-start 250
build/debug/devctl --socket /tmp/devmgrd.sock telemetry
build/debug/devctl --socket /tmp/devmgrd.sock upgrade firmware.bin 2.0.0
```

## Fault Injection

```sh
build/debug/device-sim --drop-rate 0.05 --corrupt-rate 0.02 --delay-ms 20 --seed 7
build/debug/device-sim --disconnect-at-percent 42
build/debug/device-sim --fail-verify
```

The percentage fault is a deterministic device-response blackout rather than a
fake fd close; physical USB removal and `/dev` renumbering remain hardware tests.

## Firmware Update and Recovery

The host validates a nonempty regular file (maximum 16 MiB), maps it read-only,
computes CRC, and sends bounded chunks. The device accepts only its next offset;
a duplicate is ACKed only if already-written bytes match. If all data response
retries expire, the host enters RECOVER, asks `FW_STATUS(session)`, validates the
reported offset, and continues. Recovery itself is capped. Details and wire
payloads are in [firmware-update.md](docs/firmware-update.md) and
[protocol.md](docs/protocol.md).

## Linux APIs Used

`open/close/read/write/fstat/mmap/munmap`, `fcntl`, `termios`, `posix_openpt`,
`grantpt/unlockpt/ptsname`, `epoll`, `timerfd`, `signalfd`, `socket/bind/listen/
accept4/connect`, `clock_gettime`, and signals masked through `sigprocmask`.

## Project Structure

```text
apps/          devmgrd, devctl, device-sim entry points
include/       public host interfaces
src/           common, protocol, transport, device, IPC, upgrade modules
simulator/     PTY device model and fault injection
firmware/      STM32F103 portable reference core
tests/         unit, stress, fuzz entry point, process integration
scripts/       demos, CI-local, sanitizer, Valgrind, coverage, benchmark
packaging/     systemd and deliberately incomplete udev examples
docs/          design, reliability, learning, debugging, and ADRs
```

## Design Decisions

- C17 keeps Linux/MCU protocol logic explicit and portable.
- A single reactor makes fd/session ownership and ordering inspectable.
- AF_UNIX avoids network exposure; the socket is owner-only by default.
- PTY provides real byte-stream/termios behavior without `socat` or root.
- Protocol structs are never serialized; every integer is encoded explicitly.
- No third-party runtime library hides the project’s learning targets.

See [ADRs](docs/adr/) for alternatives and consequences.

## Known Limitations

- The daemon currently manages one configured transport, not hotplug discovery
  or multiple devices.
- Firmware mapping/CRC runs on one bounded validation worker. The worker owns no
  session or transport state; it returns an immutable result to the reactor via
  eventfd, where ownership is transferred explicitly.
- Simulator flash is memory-backed and does not model erase timing/power loss.
- No authentication or firmware signature exists; local socket permissions and
  CRC provide integrity against accidents, not malicious firmware.
- Host + simulator are CI verified. STM32 portable core is cross-compile
  verified. STM32 hardware integration is not yet verified.

## Roadmap

1. Multiple device sessions and udev-driven hotplug/reconnect.
2. Signed image metadata, anti-rollback, and persistent operation journal.
3. Privilege separation for firmware file validation.
4. Real STM32 board port and power-cut test fixture.

No performance number is claimed here; run `scripts/benchmark.sh` on the target
machine and record the environment with the result.
