# Repository Engineering Rules

## Architecture

- Keep protocol independent of serial/PTY and IPC.
- The reactor thread owns all fds and device/session state.
- Worker threads may not mutate reactor-owned objects.
- Encode wire fields explicitly; never serialize C structs.
- State transitions go through named transition functions.

## Coding Style

- C17, four spaces, 100-column preference, no compiler extensions in common code.
- Public names use the `devmgr_` prefix; internal helpers are `static`.
- Keep ownership visible in names, APIs, and comments. Avoid mutable globals.
- Check integer arithmetic before allocating or indexing.

## Build and Test

```sh
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug --output-on-failure
cmake --preset asan
cmake --build --preset asan -j
ctest --preset asan --output-on-failure
```

The convenience targets are `make build`, `make test`, `make demo`, and
`make clean`. CMake remains the only build system.

## No-Toy Policy

- Never assume one `read()` returns one frame.
- Handle partial I/O, EINTR, EAGAIN/EWOULDBLOCK, disconnects, and bounded retries.
- Do not add fake success paths, core TODOs, unbounded sleeps, or placeholder tests.
- Future work belongs in the roadmap and must not be advertised as implemented.

## Dependencies

Prefer libc, POSIX, pthreads, and Linux syscalls. A new runtime dependency needs
an ADR showing why a small in-tree implementation is inappropriate.

## Error Handling

- Return stable negative `devmgr_status` values from portable libraries.
- Preserve errno at syscall boundaries when diagnostics need it.
- Close/free in the reverse order of acquisition; shutdown paths are tested.
- Log enough device, sequence, state, and operation context to diagnose failures.

## Documentation

Update protocol and state-machine documents in the same change as behavior.
Never claim benchmarks, sanitizer results, or hardware tests that were not run.

## Review Checklist

- Bounds and overflow; allocation and fd lifetime; partial I/O and EINTR
- State transition validity; timeout/retry bounds; sequence correlation
- Parser resynchronization; upgrade idempotency/resume; clean SIGTERM shutdown
- Race/deadlock risks; tests for happy, failure, boundary, and recovery paths

