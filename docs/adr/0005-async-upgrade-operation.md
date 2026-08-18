# ADR 0005: Daemon-Owned Asynchronous Upgrade Operations

## Context

Firmware update can outlive ordinary request/retry windows. Holding one client
socket for the entire operation tied device recovery to an unrelated IPC
lifetime and made CLI disconnect ownership ambiguous.

## Decision

Starting an upgrade returns an operation ID and closes that IPC connection. The
daemon owns image mapping, state, retry/recovery, and terminal result. Clients
poll state/result/offset/total using short status requests. Individual IPC has a
five-second liveness bound but no socket imposes an operation-wide deadline.

## Alternatives

A long synchronous IPC stream is simpler but couples lifetimes. Cancelling on
client disconnect can leave device flash mid-update. A persistent database/job
framework is disproportionate for this project.

## Consequences

CLI exit does not cancel a safe operation, and fault blackout can exceed a local
exchange without causing false failure. Current operation status is in memory;
daemon crash persistence/journaling remains future work.

