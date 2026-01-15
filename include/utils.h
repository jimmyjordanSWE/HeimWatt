#ifndef HEIMWATT_UTILS_H
#define HEIMWATT_UTILS_H

#include <stddef.h>
#include <stdlib.h>

#include "types.h"

// --- Safe Memory Macros ---
#define SAFE_FREE(ptr)    \
    do {                  \
        if ((ptr)) {      \
            free((ptr));  \
            (ptr) = NULL; \
        }                 \
    } while (0)

/**
 * @brief Thread-safe malloc wrapper.
 * Zeros memory after allocation.
 * @param ptr Pointer variable to allocate.
 */
#define SAFE_ALLOC(ptr)                              \
    do {                                             \
        (ptr) = malloc(sizeof(*(ptr)));              \
        if ((ptr)) memset((ptr), 0, sizeof(*(ptr))); \
    } while (0)

// --- Assert/Panic ---

/**
 * @brief Panic macro to print error and exit.
 * @param fmt Format string.
 * @param ... Arguments.
 */
#define PANIC(fmt, ...)                                                                 \
    do {                                                                                \
        fprintf(stderr, "[PANIC] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                                                             \
    } while (0)

// --- Logging ---
#include "log.h"

// --- Time Helpers ---

/**
 * @brief Generate a thread-safe timestamp string.
 * @param t Time to format.
 * @param buf Buffer to write to.
 * @param size Buffer size.
 */
void get_time_str(time_t t, char* buf, size_t size);

// --- Signal Handling ---

/**
 * @brief Setup signal handlers (SIGINT, SIGTERM).
 */
void setup_signals(void);

/**
 * @brief Check if shutdown signal was received.
 * @return true if shutdown requested.
 */
bool is_shutdown_requested(void);

/**
 * @brief Manually request shutdown (simulate signal).
 */
void request_shutdown(void);

#endif  // HEIMWATT_UTILS_H
