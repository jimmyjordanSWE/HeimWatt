

#include "utils.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t g_shutdown_requested = 0;

void log_msg(log_level level, const char* fmt, ...) {
    const char* level_strs[] = {"DEBUG", "INFO", "WARN", "ERROR"};

    // Simple timestamp using thread-safe localtime_r
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char time_buf[TIME_STR_LEN];
    (void)strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_info);

    // Format message
    va_list args;
    va_start(args, fmt);

    (void)fprintf(stderr, "[%s] [%s] ", time_buf, level_strs[level]);
    (void)vfprintf(stderr, fmt, args);  // NOLINT(clang-analyzer-valist.Uninitialized)
    (void)fprintf(stderr, "\n");

    va_end(args);
}

void get_time_str(time_t time_val, char* buf, size_t size) {
    struct tm tm_info;
    localtime_r(&time_val, &tm_info);
    (void)strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm_info);
}

static void signal_handler(int signo) {
    (void)signo;
    // Keep it async-signal-safe!
    g_shutdown_requested = 1;
}

void setup_signals(void) {
    struct sigaction sa_term = {0};
    sa_term.sa_handler = signal_handler;
    sigaction(SIGINT, &sa_term, NULL);
    sigaction(SIGTERM, &sa_term, NULL);

    // Ignore SIGPIPE (client disconnect during write)
    struct sigaction sa_pipe = {0};
    sa_pipe.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa_pipe, NULL);
}

bool is_shutdown_requested(void) { return g_shutdown_requested != 0; }

void request_shutdown(void) { g_shutdown_requested = 1; }
