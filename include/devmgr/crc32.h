#ifndef DEVMGR_CRC32_H
#define DEVMGR_CRC32_H

#include <stddef.h>
#include <stdint.h>

#define DEVMGR_CRC32_INIT UINT32_C(0xFFFFFFFF)
#define DEVMGR_CRC32_XOR_OUT UINT32_C(0xFFFFFFFF)

uint32_t devmgr_crc32_update(uint32_t state, const void *data, size_t length);
uint32_t devmgr_crc32_finish(uint32_t state);
uint32_t devmgr_crc32(const void *data, size_t length);

#endif

