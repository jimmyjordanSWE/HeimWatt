#include "log_structured.h"

#include <stdarg.h>
#include <stdio.h>

void log_event_impl(int level, const char* file, int line, const char* category, const char* event,
                    const char* fmt, ...) {
    // We want to format the message into a buffer first
    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    log_log(level, file, line, "[%s] %s: %s", category, event, msg_buf);
}
