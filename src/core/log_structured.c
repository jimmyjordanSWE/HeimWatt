#include "log_structured.h"

#include <stdarg.h>
#include <stdio.h>

void log_event_impl(int level, const char *file, int line, const char *category, const char *event,
                    const char *fmt, ...)
{
    // We want to format the message into a buffer first
    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    // Call the underlying log library
    // The underlying library (log.c) takes (level, file, line, fmt, ...)
    // We format our message as: "[CATEGORY] event: message"
    // Or we rely on the structured nature?
    // The plan says: log_event(LOG_INFO, "plugin", "started", "{\"id\":\"%s\"}", id);
    // Which produces structured data in the message part.

    // We'll format the output string to include category and event for human readability in stdout
    // log_log(level, file, line, "[%s] %s: %s", category, event, msg_buf);

    // Note: rxi/log.c uses macros log_trace, log_debug etc which call log_log.
    // We can call log_log directly if exposed, or use the level.

    log_log(level, file, line, "[%s] %s: %s", category, event, msg_buf);
}
