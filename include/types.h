#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdint.h>

// --- Constants ---
enum {
    MAX_BUFFER_SIZE = 4096,
    MAX_LOG_MSG = 1024,
    SECS_PER_HOUR = 3600,
    HOURS_PER_DAY = 24,
    DEFAULT_PORT = 8080
};

/**
 * @brief Log severity levels.
 */
typedef enum { LOG_LEVEL_DEBUG = 0, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR } log_level;

#endif  // TYPES_H
