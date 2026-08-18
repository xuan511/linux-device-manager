# Verification Boundary

```text
Host Protocol
     ↕
same versioned wire specification
     ↕
STM32 portable protocol core

arm-none-eabi-gcc
       ↓
compile PASS
```

| Scope | Status |
|---|---|
| Host + Simulator | verified by CI |
| STM32 portable core | cross-compile verified |
| STM32 hardware integration | not yet verified |

Cross-compile verification covers explicit little-endian encoding, the same
CRC32 parameters and frame layout, heap-free update state, idempotent chunks,
and FW_STATUS offset reporting as Cortex-M3 freestanding objects. It does not
cover a board-specific HAL, UART ISR/DMA, startup/linker files, flash geometry,
atomic boot metadata, watchdog behavior, image jump, electrical behavior, or
power-loss testing. Those require the chosen board and physical validation.

