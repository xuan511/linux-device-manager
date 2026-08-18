# State Machines

## Device Session

```mermaid
stateDiagram-v2
    [*] --> DISCONNECTED
    DISCONNECTED --> CONNECTING: connect
    CONNECTING --> HANDSHAKING: transport connected
    HANDSHAKING --> READY: handshake accepted
    READY --> STREAMING: start telemetry
    STREAMING --> READY: stop telemetry
    READY --> UPGRADING: begin upgrade
    UPGRADING --> READY: upgrade complete
    READY --> REBOOTING: reboot
    STREAMING --> REBOOTING: reboot
    UPGRADING --> REBOOTING: activate/reboot
    REBOOTING --> HANDSHAKING: transport reconnected
    CONNECTING --> DISCONNECTED: failure / disconnect
    HANDSHAKING --> DISCONNECTED: failure / disconnect
    READY --> DISCONNECTED: failure / disconnect
    STREAMING --> DISCONNECTED: failure / disconnect
    UPGRADING --> DISCONNECTED: failure / disconnect
    REBOOTING --> DISCONNECTED: failure / disconnect
```

All changes pass through `devmgr_session_transition`; invalid edges return
`DEVMGR_ERROR_STATE`. A transport loss cancels any in-flight request. Upgrade
recovery later reconnects through the normal handshake and queries `FW_STATUS`.

## Request Lifecycle

Only one protocol request is in flight per serial device. The session assigns a
nonzero sequence, stores an owned copy, and records a monotonic deadline. A
matching response completes it. Wrong sequences and unexpected/duplicate
responses are counted but never complete another request. At deadline the exact
request is resent with the retry flag; after the bounded policy is exhausted it
returns timeout and clears pending state.

## Upgrade State Machine

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> VALIDATE_IMAGE: start
    VALIDATE_IMAGE --> ENTER_BOOTLOADER: valid image
    ENTER_BOOTLOADER --> BEGIN: handshake
    BEGIN --> TRANSFER: session accepted
    TRANSFER --> TRANSFER: chunk acknowledged
    TRANSFER --> END: all bytes acknowledged
    END --> VERIFY
    VERIFY --> ACTIVATE: CRC matches
    ACTIVATE --> REBOOT
    REBOOT --> WAIT_RECONNECT
    WAIT_RECONNECT --> CONFIRM_VERSION
    CONFIRM_VERSION --> COMPLETE
    TRANSFER --> RECOVER: disconnect / retry exhausted
    RECOVER --> TRANSFER: FW_STATUS offset
    VALIDATE_IMAGE --> ERROR: invalid image
    VERIFY --> ERROR: mismatch
    RECOVER --> ERROR: recovery exhausted
```

The diagram is normative for Phase 7; its implementation remains separate from
device connectivity because upgrade progress survives a transport reconnect.

