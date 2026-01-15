#include <ctype.h>
#include <heimwatt_sdk.h>
#include <stdlib.h>
#include <string.h>

#include "sdk_internal.h"

int sdk_register_ticker(plugin_ctx* ctx, sdk_tick_handler handler) {
    if (!ctx || !handler) return -1;
    if (ctx->ticker_count >= SDK_MAX_TICKERS) return -1;

    // Spec assumes ticker is based on manifest interval?
    // Actually, spec says: "Registers handler for the interval_seconds defined in manifest."
    // But how do we know the manifest interval?
    // We might need to fetch it from config? "manifest.schedule"?
    // Or we assume the user passes explicit interval?
    // The spec v3.0 says: `int sdk_register_ticker(plugin_ctx* ctx, sdk_tick_handler handler);`
    // This implies looking up "interval_seconds" from config.

    // Let's assume for now we look up "interval" from config.
    // Since config lookup might require IPC which might not be connected yet (if called before
    // run), we have a chicken and egg problem unless config is avail immediately or we defer
    // lookup. BUT! sdk_create parses args. Config is fetched via IPC. If we call register before
    // run, we can't fetch config yet.

    // Workaround: Store the intent, resolve interval in sdk_run() startup.
    // For this implementation, I'll store it with interval=0, and fix it in sdk_run or just use a
    // default? Actually, usually `sdk_register_ticker` takes an interval. Re-reading spec v3.0:
    // "Registers handler for the interval_seconds defined in manifest."
    // OK, so we rely on Core to tell us the interval, OR we look it up.

    sdk_ticker_entry* e = &ctx->tickers[ctx->ticker_count++];
    e->handler = handler;
    e->is_cron = false;
    e->interval_sec = 60;  // Default fallback?
    e->next_run = -1;      // Will be scheduled in run loop

    return 0;
}

int sdk_register_cron(plugin_ctx* ctx, const char* expr, sdk_tick_handler handler) {
    if (!ctx || !expr || !handler) return -1;
    if (ctx->ticker_count >= SDK_MAX_TICKERS) return -1;

    sdk_ticker_entry* e = &ctx->tickers[ctx->ticker_count++];
    e->handler = handler;
    e->is_cron = true;
    e->cron_expr = strdup(expr);
    e->next_run = -1;

    return 0;
}

int sdk_register_fd(plugin_ctx* ctx, int fd, sdk_io_handler handler) {
    if (!ctx || fd < 0 || !handler) return -1;
    if (ctx->fd_count >= SDK_MAX_FDS) return -1;

    sdk_fd_entry* e = &ctx->m_fds[ctx->fd_count++];
    e->fd = fd;
    e->handler = handler;

    return 0;
}

int64_t sdk_time_now(void) { return (int64_t)time(NULL); }

int64_t sdk_time_parse_iso(const char* str) {
    if (!str) return 0;
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    // Simple ISO 8601 parser (YYYY-MM-DDTHH:MM:SSZ)
    // strptime is POSIX
    if (strptime(str, "%Y-%m-%dT%H:%M:%S", &tm) == NULL) {
        return 0;
    }
    return (int64_t)timegm(&tm);  // UTC
}
