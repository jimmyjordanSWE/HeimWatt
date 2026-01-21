#ifndef HEIMWATT_LOG_STRUCTURED_H
#define HEIMWATT_LOG_STRUCTURED_H

#include "libs/log.h"

// Macro to capture correct file/line from call site
#define log_event(level, category, event, fmt, ...) \
    log_event_impl(level, __FILE__, __LINE__, category, event, fmt, ##__VA_ARGS__)

void log_event_impl(int level, const char *file, int line, const char *category, const char *event,
                    const char *fmt, ...);

#endif /* HEIMWATT_LOG_STRUCTURED_H */
