#ifndef HEIMWATT_LOG_RING_H
#define HEIMWATT_LOG_RING_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Represents a single log entry in the ring buffer.
 */
typedef struct log_entry
{
    int64_t timestamp;
    int level;
    char category[32];
    char event[64];
    char message[256];
} log_entry;

/**
 * @brief Initialize the global log ring buffer.
 *
 * @param capacity Number of entries to store (e.g., 100).
 */
void log_ring_init(size_t capacity);

/**
 * @brief Push a new entry to the ring buffer.
 *
 * Thread-safe. Overwrites oldest entry if full.
 *
 * @param entry Pointer to entry data (will be copied).
 */
void log_ring_push(const log_entry *entry);

/**
 * @brief Retrieve recent log entries.
 *
 * Thread-safe.
 *
 * @param out Buffer to write entries to.
 * @param max_count Capacity of the out buffer.
 * @return Number of entries written.
 */
size_t log_ring_get_recent(log_entry *out, size_t max_count);

/**
 * @brief Export recent logs as a JSON string.
 *
 * Returns a malloc'd string containing a JSON object with a "logs" array.
 * Caller must free the result.
 *
 * @param max_count Maximum number of entries to include.
 * @return JSON string or NULL on allocation failure.
 */
char *log_ring_to_json(size_t max_count);

#endif /* HEIMWATT_LOG_RING_H */
