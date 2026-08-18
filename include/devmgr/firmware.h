#ifndef DEVMGR_FIRMWARE_H
#define DEVMGR_FIRMWARE_H

#include <stddef.h>
#include <stdint.h>

#define DEVMGR_FIRMWARE_MAX_SIZE (16U * 1024U * 1024U)

struct devmgr_firmware_image {
    int fd;
    const uint8_t *data;
    size_t size;
    uint32_t crc32;
};

int devmgr_firmware_open(struct devmgr_firmware_image *image, const char *path);
void devmgr_firmware_close(struct devmgr_firmware_image *image);

#endif

