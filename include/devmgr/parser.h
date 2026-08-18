#ifndef DEVMGR_PARSER_H
#define DEVMGR_PARSER_H

#include "devmgr/protocol.h"
#include "devmgr/ring_buffer.h"

#include <stddef.h>
#include <stdint.h>

#define DEVMGR_PARSER_CAPACITY (DEVMGR_PROTOCOL_MAX_FRAME * 2U)

struct devmgr_parser_stats {
    uint64_t bytes_received;
    uint64_t frames_decoded;
    uint64_t crc_errors;
    uint64_t length_errors;
    uint64_t discarded_bytes;
    uint64_t overflow_bytes;
};

typedef int (*devmgr_frame_callback)(const struct devmgr_frame *frame, void *context);

struct devmgr_parser {
    struct devmgr_ring_buffer input;
    uint8_t storage[DEVMGR_PARSER_CAPACITY];
    struct devmgr_parser_stats stats;
};

int devmgr_parser_init(struct devmgr_parser *parser);
void devmgr_parser_reset(struct devmgr_parser *parser);
int devmgr_parser_feed(struct devmgr_parser *parser, const void *data, size_t length,
                       devmgr_frame_callback callback, void *context, size_t *frames_emitted);
const struct devmgr_parser_stats *devmgr_parser_get_stats(const struct devmgr_parser *parser);

#endif

