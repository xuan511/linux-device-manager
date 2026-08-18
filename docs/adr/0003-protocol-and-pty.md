# ADR 0003: Binary Protocol over Serial or PTY

## Context

The same host stack must work with real UART hardware and a deterministic,
unprivileged simulator.

## Decision

Use a versioned little-endian binary frame with explicit encoding and CRC32.
Abstract byte-stream transport; production uses termios serial and tests use a
PTY created directly with POSIX APIs.

## Alternatives

A line protocol is easy to inspect but does not exercise robust framing or
binary firmware transfer. `socat` adds an avoidable runtime/test dependency.

## Consequences

The parser must support arbitrary fragmentation, coalescing, corruption, and
resynchronization. A hex-dump logging mode is needed for debugging.

