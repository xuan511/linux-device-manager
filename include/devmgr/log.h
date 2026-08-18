#ifndef DEVMGR_LOG_H
#define DEVMGR_LOG_H

#include <stdarg.h>

enum devmgr_log_level {
    DEVMGR_LOG_ERROR = 0,
    DEVMGR_LOG_WARN,
    DEVMGR_LOG_INFO,
    DEVMGR_LOG_DEBUG,
    DEVMGR_LOG_TRACE
};

void devmgr_log_set_level(enum devmgr_log_level level);
enum devmgr_log_level devmgr_log_get_level(void);
void devmgr_log_write(enum devmgr_log_level level, const char *component, const char *format, ...);
void devmgr_log_vwrite(enum devmgr_log_level level, const char *component, const char *format,
                       va_list args);

#endif

