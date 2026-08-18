# Device Management Protocol v1

## Wire Frame

All integers are little endian. C structs are never copied to the wire.

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 2 | magic | `0x4D44` (bytes `44 4d`) |
| 2 | 1 | version | `1` |
| 3 | 1 | type | command/response identifier |
| 4 | 1 | flags | response=bit0, ACK-required=bit1, retry=bit2 |
| 5 | 1 | reserved | must be zero |
| 6 | 4 | sequence | request correlation identifier |
| 10 | 2 | length | payload bytes, maximum 4096 |
| 12 | N | payload | command-specific bytes |
| 12+N | 4 | CRC32 | frame bytes 0 through 11+N |

CRC32 is the reflected ISO-HDLC/PKZIP variant: polynomial `0x04C11DB7`
(`0xEDB88320` reflected), initial value and xor-out `0xFFFFFFFF`, input and
output reflected. The check value for ASCII `123456789` is `0xCBF43926`.

## Stream Rules

Serial and PTY are byte streams: reads may contain a fragment, one frame, or
multiple frames. Receivers scan for magic, validate version/reserved/length,
wait for the complete frame, then validate CRC. On an invalid header or CRC they
discard one byte and resume scanning. Payloads above 4096 are never buffered as
frames.

## Sequence and Retry

A requester allocates a nonzero monotonically increasing sequence. Responses,
ACK, and NACK repeat it. A retry repeats both sequence and command and sets the
retry flag. Devices cache the most recent idempotent result per session so a
lost ACK does not repeat a flash write. Stale or unexpected sequences are
counted and ignored.

## Command Registry

| ID | Command | Request | Response | State | Timeout / retry |
|---:|---|---|---|---|---|
| 01 | PING | optional token | same token | ready+ | 500 ms / 2 |
| 02 | GET_INFO | empty | versioned device info | ready+ | 1 s / 2 |
| 03 | GET_HEALTH | empty | temperature/voltage/flags | ready+ | 1 s / 2 |
| 04 | GET_STATS | empty | protocol counters | ready+ | 1 s / 2 |
| 05 | START_TELEMETRY | interval ms u32 | ACK | ready | 1 s / 2 |
| 06 | STOP_TELEMETRY | empty | ACK | streaming | 1 s / 2 |
| 10 | ENTER_BOOTLOADER | empty | ACK | ready | 3 s / 1 |
| 11 | FW_BEGIN | size/crc/version/chunk | session/offset | bootloader | 2 s / 3 |
| 12 | FW_DATA | session/offset/data | next offset | upgrading | 2 s / 5 |
| 13 | FW_STATUS | session | next offset/state | bootloader+ | 2 s / 3 |
| 14 | FW_END | session | ACK | upgrading | 3 s / 3 |
| 15 | FW_VERIFY | session | calculated CRC | upgrading | 10 s / 1 |
| 16 | FW_ACTIVATE | session | ACK | verified | 3 s / 1 |
| 17 | REBOOT | mode | ACK | ready+ | 3 s / 1 |

Response payload schemas and complete firmware examples are extended alongside
their implementing phases. NACK/ERROR begins with a little-endian u16 error code:
invalid-command=1, invalid-state=2, invalid-payload=3, CRC=4, offset=5,
storage=6, verify=7, busy=8, internal=9.

Telemetry frames use type `0x83`, response flag, and sequence zero because they
are asynchronous rather than a response. Their 16-byte payload is temperature
in signed milli-Celsius, voltage in millivolts, uptime seconds, and sample
counter (four little-endian 32-bit fields). START_TELEMETRY accepts an interval
from 100 to 60000 ms.
