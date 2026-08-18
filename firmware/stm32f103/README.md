# STM32F103 Reference Port

This directory is a build-verified reference core, not a flash-ready board
firmware. It provides the exact little-endian frame/CRC implementation, a
heap-free bootloader update state machine, idempotent duplicate chunks,
FW_STATUS resume, verification, activation hooks, and minimal application
PING/GET_INFO handling.

Build the Cortex-M3 objects with:

```sh
./scripts/build-stm32-reference.sh
```

To port it to a board, implement `stm32_flash_ops` using the selected STM32 HAL
or register layer; connect the codec to interrupt/DMA UART RX and a streaming
buffer; provide startup code, clock configuration, watchdog handling, memory
map, vector table, linker scripts, image metadata/rollback policy, and an
atomic boot flag. Flash erase/program alignment and interrupt exclusion must
match the exact MCU density and board.

Verified here: C17 sources cross-compile with `arm-none-eabi-gcc`, Cortex-M3,
freestanding, strict warnings-as-errors. Not verified here: linking, flashing,
power-loss behavior, UART electrical/timing behavior, option bytes, boot jump,
watchdog/reset cause, or any physical STM32F103 board. Those claims require
the actual hardware and selected BSP.

