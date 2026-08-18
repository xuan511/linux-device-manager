# Reliability and Recovery

## Byte Streams and Corruption

All fds are nonblocking. Reads drain until EAGAIN and feed a persistent ring
buffer; writes retain their offset until complete. EINTR is retried. The parser
accepts any fragmentation/coalescing and discards one byte after invalid header
or CRC so later valid magic can resynchronize. Payload and IPC lengths are
bounded before buffering.

For a noncanonical TTY configured with `VMIN=0, VTIME=0`, a drained read may
return zero even while the PTY master is alive. The daemon treats that as "no
bytes now"; transport loss comes from EPOLLHUP/EPOLLERR or a real I/O error.
EPOLLRDHUP is requested only for Unix stream clients, not TTY descriptors. The
simulator process exclusively owns the PTY master from creation through cleanup,
and integration harnesses assert both background processes remain alive until
READY rather than merely waiting for a socket pathname.

## Timeout, Retry, and Duplication

Requests carry a nonzero sequence and use CLOCK_MONOTONIC deadlines. The session
keeps one owned request copy, repeats it with the retry flag, and stops at the
policy limit. Wrong, stale, duplicate, and unsolicited sequences cannot complete
the pending request. Firmware chunks add session+offset idempotency: replayed
data is ACKed only when it exactly matches already committed bytes.

## Firmware Resume

If all FW_DATA retries expire, the upgrade does not reset offset. It enters
RECOVER and asks `FW_STATUS(session)`. The device returns session and next
expected offset; the host validates the bounds and resumes from that offset.
Recovery is capped at three cycles. A mismatch, NACK, invalid offset, or further
exhaustion moves to ERROR and releases the mapped image.

## Simulator Fault Injection

```sh
device-sim --drop-rate 0.05 --corrupt-rate 0.02 --delay-ms 20 --seed 7
device-sim --disconnect-at-percent 42
device-sim --fail-verify
```

Drop and corruption use a repeatable xorshift stream. Delay is applied to
responses. `--disconnect-at-percent` creates a six-second device-side response
blackout at the selected firmware percentage; this deterministically exhausts
normal data retries and exercises FW_STATUS resume while preserving the PTY
path. It models a link outage, not a kernel-level unplug. Physical USB removal
and `/dev` renumbering remain hardware/udev integration tests.

## Shutdown and Reboot

SIGINT/SIGTERM are blocked and consumed through signalfd, ordering shutdown with
other reactor work. Cleanup closes clients, transport, signal/timer/listener/
epoll fds, unmaps firmware, and unlinks only the configured daemon socket. The
simulator frees emulated flash. Device reboot keeps the PTY in the simulator;
real hardware reconnect behavior needs physical validation.
