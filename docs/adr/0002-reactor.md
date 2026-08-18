# ADR 0002: epoll Reactor with a Bounded Worker

## Context

Serial, IPC, timers, signals, and worker completion must coexist without a busy
loop or a thread per connection.

## Decision

Use one edge-aware epoll reactor owning file descriptors and mutable sessions.
Use timerfd for deadlines, signalfd for lifecycle signals, and eventfd for a
bounded worker's completion notifications.

## Alternatives

`select` has awkward limits and scanning cost. A thread per device complicates
ordering and state ownership. libevent/libuv would hide the APIs being learned.

## Consequences

Callbacks must drain nonblocking descriptors. CPU-heavy work cannot run on the
reactor. Cross-thread results require explicit ownership transfer.

