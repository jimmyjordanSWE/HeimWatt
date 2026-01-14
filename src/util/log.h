/**
 * @file log.h
 * @brief Logging utilities
 *
 * Provides logging functions for the HeimWatt system.
 */

#ifndef HEIMWATT_LOG_H
#define HEIMWATT_LOG_H

/**
 * Log levels.
 */
typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } log_level;

/**
 * Log a message.
 *
 * @param level Log level
 * @param fmt   printf-style format string
 * @param ...   Format arguments
 */
void log_msg(log_level level, const char* fmt, ...);

/**
 * Set minimum log level.
 * Messages below this level will be discarded.
 *
 * @param level Minimum level to log
 */
void log_set_level(log_level level);

/**
 * Set log output file.
 * If not set, logs go to stderr.
 *
 * @param path Path to log file (NULL for stderr)
 * @return 0 on success, -1 on error
 */
int log_set_file(const char* path);

/**
 * Close log file if open.
 */
void log_close(void);

/* Convenience macros */
#define LOG_DEBUG(...) log_msg(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) log_msg(LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...) log_msg(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) log_msg(LOG_ERROR, __VA_ARGS__)

#endif /* HEIMWATT_LOG_H */
