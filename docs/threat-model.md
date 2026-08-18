# Threat Model

## Assets and Boundaries

Assets are firmware image bytes/version, device availability, serial ownership,
upgrade progress, and the local management API. Boundaries are the owner-only
AF_UNIX socket, serial/USB physical link, firmware path supplied by a local
user, and image bytes crossing into bootloader flash.

## Defended Failures

- Malformed lengths are rejected before copies; protocol/IPC payloads are capped.
- CRC detects accidental transport corruption; sequence and offset prevent stale
  responses or duplicate flash writes from advancing state.
- Firmware open uses `O_NOFOLLOW`, regular-file and size checks, read-only mmap.
- Default socket mode is `0600`; systemd runs an unprivileged dedicated account.
- Retry/recovery counts are finite, preventing an unreachable device from
  consuming the reactor indefinitely.
- No network listener exists; packaged service restricts socket families.

## Explicit Non-Goals / Open Risks

CRC is not authenticity. A user able to access the socket and firmware file can
request arbitrary unsigned code. There is no signature, encryption, secure
boot integration, anti-rollback counter, privilege-separated file worker, or
persistent tamper-resistant operation journal. Physical UART attackers can
observe/inject bytes. Simulator fault injection is reliability testing, not an
adversarial security proof.

Production hardening requires signed manifests, a pinned public key in trusted
bootloader storage, version monotonicity, rollback slots, authenticated local
authorization, durable audit records, rate limits, and board-specific secure
boot/readout protection. Paths and udev rules must be administrator-reviewed.

