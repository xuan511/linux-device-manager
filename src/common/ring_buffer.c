#include "devmgr/ring_buffer.h"

#include "devmgr/error.h"

#include <string.h>

static size_t minimum(size_t left, size_t right)
{
    return left < right ? left : right;
}

int devmgr_ring_init(struct devmgr_ring_buffer *ring, uint8_t *storage, size_t capacity)
{
    if (ring == NULL || storage == NULL || capacity == 0U) {
        return DEVMGR_ERROR_INVALID;
    }
    ring->storage = storage;
    ring->capacity = capacity;
    ring->read_pos = 0U;
    ring->size = 0U;
    return DEVMGR_OK;
}

void devmgr_ring_reset(struct devmgr_ring_buffer *ring)
{
    if (ring != NULL) {
        ring->read_pos = 0U;
        ring->size = 0U;
    }
}

size_t devmgr_ring_size(const struct devmgr_ring_buffer *ring)
{
    return ring == NULL ? 0U : ring->size;
}

size_t devmgr_ring_space(const struct devmgr_ring_buffer *ring)
{
    return ring == NULL ? 0U : ring->capacity - ring->size;
}

bool devmgr_ring_empty(const struct devmgr_ring_buffer *ring)
{
    return devmgr_ring_size(ring) == 0U;
}

bool devmgr_ring_full(const struct devmgr_ring_buffer *ring)
{
    return ring != NULL && ring->size == ring->capacity;
}

size_t devmgr_ring_write(struct devmgr_ring_buffer *ring, const void *data, size_t length)
{
    if (ring == NULL || data == NULL || length == 0U) {
        return 0U;
    }
    size_t count = minimum(length, devmgr_ring_space(ring));
    size_t write_pos = (ring->read_pos + ring->size) % ring->capacity;
    size_t first = minimum(count, ring->capacity - write_pos);
    memcpy(ring->storage + write_pos, data, first);
    memcpy(ring->storage, (const uint8_t *)data + first, count - first);
    ring->size += count;
    return count;
}

size_t devmgr_ring_peek(const struct devmgr_ring_buffer *ring, size_t offset, void *data,
                        size_t length)
{
    if (ring == NULL || data == NULL || offset >= ring->size || length == 0U) {
        return 0U;
    }
    size_t count = minimum(length, ring->size - offset);
    size_t position = (ring->read_pos + offset) % ring->capacity;
    size_t first = minimum(count, ring->capacity - position);
    memcpy(data, ring->storage + position, first);
    memcpy((uint8_t *)data + first, ring->storage, count - first);
    return count;
}

size_t devmgr_ring_discard(struct devmgr_ring_buffer *ring, size_t length)
{
    if (ring == NULL || length == 0U) {
        return 0U;
    }
    size_t count = minimum(length, ring->size);
    ring->read_pos = (ring->read_pos + count) % ring->capacity;
    ring->size -= count;
    if (ring->size == 0U) {
        ring->read_pos = 0U;
    }
    return count;
}

size_t devmgr_ring_read(struct devmgr_ring_buffer *ring, void *data, size_t length)
{
    size_t count = devmgr_ring_peek(ring, 0U, data, length);
    (void)devmgr_ring_discard(ring, count);
    return count;
}

