/**
 * @file ss_log.c
 * @brief ss_log implementation.
 */

#include "ss_log.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_PATH_LEN 1024
#define MAX_TIMESTAMP_LEN 64

static struct {
    FILE* file;
    bool console_enabled;
    bool console_color;
    size_t file_max_bytes;
    char file_base_path[MAX_PATH_LEN];
    char file_current[MAX_PATH_LEN + MAX_TIMESTAMP_LEN];
    ss_lock_fn lock_fn;
    void* lock_udata;
    bool lock_enabled;
    pthread_mutex_t internal_mutex;
    int initialized;
} log_settings;

static const char* level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static const char* level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m",
                                     "\x1b[33m", "\x1b[31m", "\x1b[35m"};

static void lock(void) {
    if (!log_settings.lock_enabled)
        return;
    if (log_settings.lock_fn)
        log_settings.lock_fn(true, log_settings.lock_udata);
    else
        pthread_mutex_lock(&log_settings.internal_mutex);
}

static void unlock(void) {
    if (!log_settings.lock_enabled)
        return;
    if (log_settings.lock_fn)
        log_settings.lock_fn(false, log_settings.lock_udata);
    else
        pthread_mutex_unlock(&log_settings.internal_mutex);
}

static void open_new_log_file(void) {
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);
    snprintf(log_settings.file_current, sizeof(log_settings.file_current), "%s.%s",
             log_settings.file_base_path, ts);
    log_settings.file = fopen(log_settings.file_current, "w");
}

static void rotate_file(void) {
    if (!log_settings.file || log_settings.file_max_bytes == 0)
        return;
    long pos = ftell(log_settings.file);
    if (pos < 0 || (size_t) pos < log_settings.file_max_bytes)
        return;
    fclose(log_settings.file);
    open_new_log_file();
}

static void log_impl(int level, const char* cat, const char* file, int line, const char* fmt,
                     va_list ap) {
    if (!log_settings.initialized)
        return;

    lock();

    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    char tbuf[32];

    rotate_file();
    if (log_settings.file) {
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
        fprintf(log_settings.file, "%s %-5s [%s] %s:%d: ", tbuf, level_strings[level], cat, file,
                line);
        va_list ap2;
        va_copy(ap2, ap);
        vfprintf(log_settings.file, fmt, ap2);
        va_end(ap2);
        fprintf(log_settings.file, "\n");
        fflush(log_settings.file);
    }

    if (log_settings.console_enabled) {
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm);
        if (log_settings.console_color) {
            fprintf(stderr, "%s %s%-5s\x1b[0m \x1b[90m[%s] %s:%d:\x1b[0m ", tbuf,
                    level_colors[level], level_strings[level], cat, file, line);
        } else {
            fprintf(stderr, "%s %-5s [%s] %s:%d: ", tbuf, level_strings[level], cat, file, line);
        }
        va_list ap2;
        va_copy(ap2, ap);
        vfprintf(stderr, fmt, ap2);
        va_end(ap2);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    unlock();
}

int ss_log_init(const ss_log_config* config) {
    if (!config || !config->file_path)
        return -EINVAL;

    memset(&log_settings, 0, sizeof(log_settings));
    log_settings.console_enabled = config->console_enabled;
    log_settings.console_color = config->console_color;
    log_settings.file_max_bytes = config->file_max_bytes;
    log_settings.lock_enabled = true;

    if (pthread_mutex_init(&log_settings.internal_mutex, NULL) != 0)
        return -ENOMEM;

    snprintf(log_settings.file_base_path, sizeof(log_settings.file_base_path), "%s",
             config->file_path);
    open_new_log_file();
    if (!log_settings.file) {
        pthread_mutex_destroy(&log_settings.internal_mutex);
        return -EIO;
    }

    log_settings.initialized = 1;
    return 0;
}

void ss_log_fini(void) {
    if (log_settings.file) {
        fclose(log_settings.file);
        log_settings.file = NULL;
    }
    pthread_mutex_destroy(&log_settings.internal_mutex);
    log_settings.initialized = 0;
}

void ss_log_lock_enable(void) {
    log_settings.lock_enabled = true;
}

void ss_log_lock_disable(void) {
    log_settings.lock_enabled = false;
}

void ss_log_set_lock(ss_lock_fn fn, void* udata) {
    log_settings.lock_fn = fn;
    log_settings.lock_udata = udata;
    log_settings.lock_enabled = true;
}

const char* ss_log_level_string(int level) {
    if (level < 0 || level > 5)
        return "UNKNOWN";
    return level_strings[level];
}

void ss_log_log_cat(int level, const char* cat, const char* file, int line, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_impl(level, cat, file, line, fmt, ap);
    va_end(ap);
}
