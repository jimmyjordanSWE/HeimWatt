/*
 * @file time_util.h
 * @brief Timestamp helpers
 *
 * Utilities for working with timestamps.
 */

#ifndef HEIMWATT_TIME_UTIL_H
#define HEIMWATT_TIME_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*
 * Get current Unix timestamp in seconds.
 *
 * @return Current timestamp
 */
int64_t time_now(void);

/*
 * Get current Unix timestamp in milliseconds.
 *
 * @return Current timestamp in ms
 */
int64_t time_now_ms(void);

/*
 * Format timestamp as ISO 8601 string.
 *
 * @param ts   Timestamp
 * @param buf  Output buffer
 * @param size Buffer size (min 20 bytes for "YYYY-MM-DDTHH:MM:SS")
 * @return 0 on success, -1 on error
 */
int time_format_iso(int64_t ts, char* buf, size_t size);

/*
 * Parse ISO 8601 string to timestamp.
 *
 * @param str ISO 8601 string
 * @return Timestamp or -1 on error
 */
int64_t time_parse_iso(const char* str);

/*
 * Get start of hour containing timestamp.
 *
 * @param ts Timestamp
 * @return Timestamp at start of hour
 */
int64_t time_floor_hour(int64_t ts);

/*
 * Get start of day containing timestamp.
 *
 * @param ts Timestamp
 * @return Timestamp at start of day (UTC)
 */
int64_t time_floor_day(int64_t ts);

#endif /* HEIMWATT_TIME_UTIL_H */
