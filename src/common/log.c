#include "devmgr/log.h"

#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

static atomic_int current_level = ATOMIC_VAR_INIT(DEVMGR_LOG_INFO);

void devmgr_log_set_level(enum devmgr_log_level level)
{
    if (level >= DEVMGR_LOG_ERROR && level <= DEVMGR_LOG_TRACE) {
        atomic_store_explicit(&current_level, level, memory_order_relaxed);
    }
}

enum devmgr_log_level devmgr_log_get_level(void)
{
    return (enum devmgr_log_level)atomic_load_explicit(&current_level, memory_order_relaxed);
}

void devmgr_log_vwrite(enum devmgr_log_level level, const char *component, const char *format,
                       va_list args)
{
    static const char *const names[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    time_t now;
    struct tm *local;
    char timestamp[32];

    if (level > devmgr_log_get_level() || level < DEVMGR_LOG_ERROR || level > DEVMGR_LOG_TRACE ||
        format == NULL) {
        return;
    }
    now = time(NULL);
    local = localtime(&now);
    if (local != NULL) {
        (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", local);
    } else {
        (void)snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }
    (void)fprintf(stderr, "%s %-5s [%s] ", timestamp, names[level],
                  component == NULL ? "core" : component);
    (void)vfprintf(stderr, format, args);
    (void)fputc('\n', stderr);
}

void devmgr_log_write(enum devmgr_log_level level, const char *component, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    devmgr_log_vwrite(level, component, format, args);
    va_end(args);
}

