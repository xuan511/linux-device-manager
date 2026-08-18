#include "devmgr/crc32.h"

uint32_t devmgr_crc32_update(uint32_t state, const void *data, size_t length)
{
    const uint8_t *bytes = data;

    if (bytes == NULL) {
        return state;
    }
    for (size_t index = 0; index < length; ++index) {
        state ^= bytes[index];
        for (unsigned bit = 0; bit < 8U; ++bit) {
            uint32_t mask = 0U - (state & 1U);
            state = (state >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return state;
}

uint32_t devmgr_crc32_finish(uint32_t state)
{
    return state ^ DEVMGR_CRC32_XOR_OUT;
}

uint32_t devmgr_crc32(const void *data, size_t length)
{
    return devmgr_crc32_finish(devmgr_crc32_update(DEVMGR_CRC32_INIT, data, length));
}

