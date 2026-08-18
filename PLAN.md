# Implementation Plan

## Architecture

The system is split into three processes: `device-sim` emulates an embedded
target over a PTY, `devmgrd` owns transports and device state, and `devctl`
talks to the daemon over a Unix domain socket. A single-threaded epoll reactor
owns file descriptors and mutable session state. A bounded worker queue handles
firmware validation and other file-heavy work. Protocol, transport, state
machines, IPC, and upgrade logic are separate libraries with explicit ownership.

## Milestones

1. Build foundation, diagnostics, CRC32, ring buffer, and unit tests.
2. Binary frame codec and resilient streaming parser.
3. Serial/PTY transport and device simulator.
4. Device session state machine, deadlines, request correlation, and retry.
5. Epoll daemon, Unix-domain IPC, and CLI.
6. Telemetry and runtime statistics.
7. Chunked firmware validation and upgrade state machine.
8. Fault injection, reconnect, and upgrade resume.
9. Integration, stress, and fuzz tests.
10. Sanitizers, Valgrind, and coverage automation.
11. CI, systemd, and udev packaging examples.
12. STM32F103 reference protocol/bootloader port.
13. Architecture, protocol, reliability, learning, and portfolio documents.
14. Full correctness, resource ownership, architecture, and test audit.

Every milestone must be built, tested, reviewed, committed, and pushed before
the next milestone is declared complete.

## Dependencies

- C17 compiler, CMake 3.20+, and pthreads
- Linux 5.x userspace APIs: epoll, timerfd, signalfd, eventfd, PTY, termios
- Optional: Ninja, lcov, Valgrind, clang-format
- No third-party runtime libraries

## Risks

- PTY timing can make integration tests flaky: synchronize through explicit
  readiness files/messages and monotonic deadlines, never arbitrary sleeps.
- Stream corruption can desynchronize framing: cap payload lengths and scan for
  magic while retaining a possible prefix byte.
- Lost firmware acknowledgements can duplicate writes: make `FW_DATA`
  idempotent by session and offset and query `FW_STATUS` after reconnect.
- Reactor/worker ownership mistakes can race: workers return immutable results
  via a protected queue and wake the reactor with eventfd.
- Physical STM32 behavior cannot be proven in CI: clearly separate build-tested
  reference firmware from hardware-validated behavior.

## Acceptance Criteria

- Debug and release builds complete without important warnings on Ubuntu 22.04
  and 24.04.
- Unit and integration tests pass; ASan and UBSan report no issues.
- The no-root demo performs discovery, info, ping, health, upgrade, verification,
  reboot, and reconnect against the PTY simulator.
- Disconnect, lost ACK, corrupt frame, and daemon shutdown paths recover cleanly.
- Documents describe the implementation as shipped, with no fake benchmark or
  hardware-validation claims.

