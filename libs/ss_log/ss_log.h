/*
 * @file ss_log.h
 * @brief Minimal C99 logging library.
 *
 * Thread-safe by default. All log calls require a category tag.
 *
 * @code
 * ss_log_config cfg = { .file_path = "app.log" };
 * ss_log_init(&cfg);
 * ss_log_info("HTTP", "Request from %s", ip);
 * ss_log_fini();
 * @endcode
 */

#ifndef SS_LOG_H
#define SS_LOG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels (0=TRACE, 5=FATAL) */
enum {
    SS_LOG_TRACE = 0,
    SS_LOG_DEBUG = 1,
    SS_LOG_INFO = 2,
    SS_LOG_WARN = 3,
    SS_LOG_ERROR = 4,
    SS_LOG_FATAL = 5
};

/* Configuration for ss_log_init() */
typedef struct {
    const char* file_path; /* Log file base path (required) */
    size_t file_max_bytes; /* Rotate at this size (0 = no rotation) */
    bool console_enabled;  /* Also log to stderr */
    bool console_color;    /* Use ANSI colors on stderr */
} ss_log_config;

/* Custom lock function signature: lock=true to acquire, lock=false to release */
typedef void (*ss_lock_fn)(bool lock, void* udata);

/*
 * @brief  Initialize the logging system.
 * @param  config  Configuration (file_path is required).
 * @return 0 on success, -EINVAL if config is invalid.
 */
int ss_log_init(const ss_log_config* config);

/*
 * @brief  Shutdown logging and close files.
 */
void ss_log_fini(void);

/*
 * @brief Enable locking (uses custom lock if set, else internal mutex).
 */
void ss_log_lock_enable(void);

/*
 * @brief Disable all locking (for single-threaded performance).
 */
void ss_log_lock_disable(void);

/*
 * @brief  Set a custom lock function.
 * @param  fn    Lock function (NULL clears custom lock, uses internal).
 * @param  udata User data passed to lock function.
 */
void ss_log_set_lock(ss_lock_fn fn, void* udata);

/*
 * @brief  Get string name for a log level.
 * @param  level  Log level (0-5).
 * @return Level name (e.g., "INFO"), or "UNKNOWN" if invalid.
 */
const char* ss_log_level_string(int level);

/* Internal - do not call directly, use macros below */
void ss_log_log_cat(int level, const char* cat, const char* file, int line, const char* fmt, ...);

/*
 * @defgroup LogMacros Logging Macros
 * @brief All macros take (category, printf_format, ...).
 * @{
 */
#define ss_log_trace(cat, ...) ss_log_log_cat(SS_LOG_TRACE, cat, __FILE__, __LINE__, __VA_ARGS__)
#define ss_log_debug(cat, ...) ss_log_log_cat(SS_LOG_DEBUG, cat, __FILE__, __LINE__, __VA_ARGS__)
#define ss_log_info(cat, ...) ss_log_log_cat(SS_LOG_INFO, cat, __FILE__, __LINE__, __VA_ARGS__)
#define ss_log_warn(cat, ...) ss_log_log_cat(SS_LOG_WARN, cat, __FILE__, __LINE__, __VA_ARGS__)
#define ss_log_error(cat, ...) ss_log_log_cat(SS_LOG_ERROR, cat, __FILE__, __LINE__, __VA_ARGS__)
#define ss_log_fatal(cat, ...) ss_log_log_cat(SS_LOG_FATAL, cat, __FILE__, __LINE__, __VA_ARGS__)
/* @} */

#ifdef __cplusplus
}
#endif

#endif /* SS_LOG_H */
