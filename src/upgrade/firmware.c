#include "devmgr/firmware.h"

#include "devmgr/crc32.h"
#include "devmgr/error.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int devmgr_firmware_open(struct devmgr_firmware_image *image, const char *path)
{
    struct stat information;
    void *mapping;
    int fd;

    if (image == NULL || path == NULL || path[0] == '\0') return DEVMGR_ERROR_INVALID;
    memset(image, 0, sizeof(*image));
    image->fd = -1;
    do {
        fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) return DEVMGR_ERROR_IO;
    if (fstat(fd, &information) < 0 || !S_ISREG(information.st_mode) || information.st_size <= 0 ||
        (uintmax_t)information.st_size > DEVMGR_FIRMWARE_MAX_SIZE) {
        (void)close(fd);
        return DEVMGR_ERROR_LIMIT;
    }
    mapping = mmap(NULL, (size_t)information.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED) {
        (void)close(fd);
        return DEVMGR_ERROR_IO;
    }
    image->fd = fd;
    image->data = mapping;
    image->size = (size_t)information.st_size;
    image->crc32 = devmgr_crc32(image->data, image->size);
    return DEVMGR_OK;
}

void devmgr_firmware_close(struct devmgr_firmware_image *image)
{
    if (image == NULL) return;
    if (image->data != NULL && image->size != 0U) (void)munmap((void *)image->data, image->size);
    if (image->fd >= 0) (void)close(image->fd);
    memset(image, 0, sizeof(*image));
    image->fd = -1;
}

