#include "devmgr/error.h"
#include "devmgr/parser.h"

#include <stddef.h>
#include <stdint.h>

static int consume(const struct devmgr_frame *frame, void *context)
{
    (void)frame;
    (void)context;
    return DEVMGR_OK;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct devmgr_parser parser;
    size_t emitted = 0U;
    if (devmgr_parser_init(&parser) == DEVMGR_OK)
        (void)devmgr_parser_feed(&parser, data, size, consume, NULL, &emitted);
    return 0;
}

