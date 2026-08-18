#include "devmgr/parser.h"

#include "devmgr/error.h"

#include <string.h>

static int parser_drain(struct devmgr_parser *parser, devmgr_frame_callback callback, void *context,
                        size_t *frames_emitted)
{
    uint8_t header[DEVMGR_PROTOCOL_HEADER_SIZE];
    uint8_t encoded[DEVMGR_PROTOCOL_MAX_FRAME];
    struct devmgr_frame frame;

    while (devmgr_ring_size(&parser->input) >= 2U) {
        (void)devmgr_ring_peek(&parser->input, 0U, header, 2U);
        if (devmgr_get_le16(header) != DEVMGR_PROTOCOL_MAGIC) {
            (void)devmgr_ring_discard(&parser->input, 1U);
            ++parser->stats.discarded_bytes;
            continue;
        }
        if (devmgr_ring_size(&parser->input) < DEVMGR_PROTOCOL_HEADER_SIZE) {
            break;
        }
        (void)devmgr_ring_peek(&parser->input, 0U, header, sizeof(header));
        if (header[2] != DEVMGR_PROTOCOL_VERSION || header[5] != 0U) {
            (void)devmgr_ring_discard(&parser->input, 1U);
            ++parser->stats.discarded_bytes;
            continue;
        }
        uint16_t payload_length = devmgr_get_le16(header + 10U);
        if (payload_length > DEVMGR_PROTOCOL_MAX_PAYLOAD) {
            (void)devmgr_ring_discard(&parser->input, 1U);
            ++parser->stats.length_errors;
            ++parser->stats.discarded_bytes;
            continue;
        }
        size_t frame_length = DEVMGR_PROTOCOL_HEADER_SIZE + payload_length + DEVMGR_PROTOCOL_CRC_SIZE;
        if (devmgr_ring_size(&parser->input) < frame_length) {
            break;
        }
        (void)devmgr_ring_peek(&parser->input, 0U, encoded, frame_length);
        int result = devmgr_frame_decode(encoded, frame_length, &frame);
        if (result == DEVMGR_ERROR_CRC) {
            (void)devmgr_ring_discard(&parser->input, 1U);
            ++parser->stats.crc_errors;
            ++parser->stats.discarded_bytes;
            continue;
        }
        if (result != DEVMGR_OK) {
            (void)devmgr_ring_discard(&parser->input, 1U);
            ++parser->stats.discarded_bytes;
            continue;
        }
        (void)devmgr_ring_discard(&parser->input, frame_length);
        ++parser->stats.frames_decoded;
        ++*frames_emitted;
        result = callback(&frame, context);
        if (result != DEVMGR_OK) {
            return result;
        }
    }
    return DEVMGR_OK;
}

int devmgr_parser_init(struct devmgr_parser *parser)
{
    if (parser == NULL) {
        return DEVMGR_ERROR_INVALID;
    }
    memset(parser, 0, sizeof(*parser));
    return devmgr_ring_init(&parser->input, parser->storage, sizeof(parser->storage));
}

void devmgr_parser_reset(struct devmgr_parser *parser)
{
    if (parser != NULL) {
        devmgr_ring_reset(&parser->input);
        memset(&parser->stats, 0, sizeof(parser->stats));
    }
}

int devmgr_parser_feed(struct devmgr_parser *parser, const void *data, size_t length,
                       devmgr_frame_callback callback, void *context, size_t *frames_emitted)
{
    const uint8_t *bytes = data;
    size_t offset = 0U;

    if (parser == NULL || (data == NULL && length != 0U) || callback == NULL ||
        frames_emitted == NULL) {
        return DEVMGR_ERROR_INVALID;
    }
    *frames_emitted = 0U;
    while (offset < length) {
        size_t written = devmgr_ring_write(&parser->input, bytes + offset, length - offset);
        parser->stats.bytes_received += written;
        offset += written;
        int result = parser_drain(parser, callback, context, frames_emitted);
        if (result != DEVMGR_OK) {
            return result;
        }
        if (written == 0U) {
            parser->stats.overflow_bytes += length - offset;
            return DEVMGR_ERROR_OVERFLOW;
        }
    }
    return parser_drain(parser, callback, context, frames_emitted);
}

const struct devmgr_parser_stats *devmgr_parser_get_stats(const struct devmgr_parser *parser)
{
    return parser == NULL ? NULL : &parser->stats;
}

