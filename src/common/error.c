#include "devmgr/error.h"

const char *devmgr_status_string(int status)
{
    switch (status) {
    case DEVMGR_OK: return "success";
    case DEVMGR_ERROR_INVALID: return "invalid argument";
    case DEVMGR_ERROR_NOMEM: return "out of memory";
    case DEVMGR_ERROR_OVERFLOW: return "overflow";
    case DEVMGR_ERROR_UNDERFLOW: return "underflow";
    case DEVMGR_ERROR_CRC: return "CRC mismatch";
    case DEVMGR_ERROR_PROTOCOL: return "protocol error";
    case DEVMGR_ERROR_TIMEOUT: return "timeout";
    case DEVMGR_ERROR_DISCONNECTED: return "disconnected";
    case DEVMGR_ERROR_IO: return "I/O error";
    case DEVMGR_ERROR_STATE: return "invalid state";
    case DEVMGR_ERROR_BUSY: return "busy";
    case DEVMGR_ERROR_NOT_FOUND: return "not found";
    case DEVMGR_ERROR_LIMIT: return "limit exceeded";
    default: return "unknown error";
    }
}

