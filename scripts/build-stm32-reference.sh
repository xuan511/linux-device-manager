#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
compiler=${ARM_CC:-arm-none-eabi-gcc}
command -v "${compiler}" >/dev/null || { echo "arm-none-eabi-gcc is required" >&2; exit 2; }
output_dir="${project_root}/build/stm32-reference"
mkdir -p "${output_dir}"
flags=(-std=c17 -mcpu=cortex-m3 -mthumb -ffreestanding -fno-builtin
       -ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic -Wconversion
       -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Werror
       -I"${project_root}/firmware/stm32f103/common"
       -I"${project_root}/firmware/stm32f103/bootloader")
"${compiler}" "${flags[@]}" -c "${project_root}/firmware/stm32f103/common/protocol.c" \
    -o "${output_dir}/protocol.o"
"${compiler}" "${flags[@]}" -c "${project_root}/firmware/stm32f103/bootloader/update.c" \
    -o "${output_dir}/update.o"
"${compiler}" "${flags[@]}" -c "${project_root}/firmware/stm32f103/application/device_app.c" \
    -o "${output_dir}/device_app.o"
"${compiler}" -print-multi-lib >/dev/null
echo "STM32 reference core cross-compiled: ${output_dir}"

