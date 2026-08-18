#ifndef DEVMGR_RING_BUFFER_H
#define DEVMGR_RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct devmgr_ring_buffer {
    uint8_t *storage;
    size_t capacity;
    size_t read_pos;
    size_t size;
};

int devmgr_ring_init(struct devmgr_ring_buffer *ring, uint8_t *storage, size_t capacity);
void devmgr_ring_reset(struct devmgr_ring_buffer *ring);
size_t devmgr_ring_size(const struct devmgr_ring_buffer *ring);
size_t devmgr_ring_space(const struct devmgr_ring_buffer *ring);
bool devmgr_ring_empty(const struct devmgr_ring_buffer *ring);
bool devmgr_ring_full(const struct devmgr_ring_buffer *ring);
size_t devmgr_ring_write(struct devmgr_ring_buffer *ring, const void *data, size_t length);
size_t devmgr_ring_read(struct devmgr_ring_buffer *ring, void *data, size_t length);
size_t devmgr_ring_peek(const struct devmgr_ring_buffer *ring, size_t offset, void *data,
                        size_t length);
size_t devmgr_ring_discard(struct devmgr_ring_buffer *ring, size_t length);

#endif

