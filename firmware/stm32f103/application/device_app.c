#include "protocol.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int stm32_application_handle(const struct stm32_frame_view *request, uint8_t *response,
                             size_t capacity, size_t *response_length);

int stm32_application_handle(const struct stm32_frame_view *request, uint8_t *response,
                             size_t capacity, size_t *response_length)
{
    static const uint8_t info[] = {0x30U, 0x4DU, 0x49U, 0x53U,
                                   'S', 'T', 'M', '3', '2', '-', 'R', 'E', 'F', 0,
                                   'H', 'W', '-', '1', '.', '0', 0,
                                   '1', '.', '0', '.', '0', 0,
                                   'B', 'L', '-', '1', '.', '0', '.', '0', 0,
                                   'R', 'E', 'F', '0', '0', '0', '0', '0', '0', '0', '1', 0};
    if (request == NULL) return -1;
    if (request->type == STM32_MSG_PING)
        return stm32_frame_encode(request->type, STM32_FRAME_RESPONSE, request->sequence,
                                  request->payload, request->payload_length, response, capacity,
                                  response_length);
    if (request->type == STM32_MSG_GET_INFO)
        return stm32_frame_encode(request->type, STM32_FRAME_RESPONSE, request->sequence,
                                  info, sizeof(info), response, capacity, response_length);
    uint8_t error[2];
    stm32_put_le16(error, 1U);
    return stm32_frame_encode(STM32_MSG_NACK, STM32_FRAME_RESPONSE, request->sequence,
                              error, sizeof(error), response, capacity, response_length);
}

