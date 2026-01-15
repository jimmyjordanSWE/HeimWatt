#ifndef SDK_INTERNAL_H
#define SDK_INTERNAL_H

#include <heimwatt_sdk.h>
#include <stdbool.h>
#include <time.h>

// Internal constants
#define SDK_IPC_BUFFER_SIZE 4096
#define SDK_MAX_TICKERS 32
#define SDK_MAX_FDS 32

// Callback wrapper types
typedef struct {
    int64_t interval_sec;
    int64_t next_run;
    sdk_tick_handler handler;
    bool is_cron;
    char* cron_expr;  // If is_cron is true
} sdk_ticker_entry;

typedef struct {
    int fd;
    sdk_io_handler handler;
} sdk_fd_entry;

// The opaque context definition
struct plugin_ctx {
    // Identity
    char* plugin_id;
    char* socket_path;

    // Connection
    int ipc_fd;
    bool connected;

    // Config cache (simple key-value list?)
    // For now, assume config is fetched on demand via IPC or cached?
    // Spec says sdk_get_config fetches from manifest via IPC typically.

    // Scheduling
    sdk_ticker_entry tickers[SDK_MAX_TICKERS];
    int ticker_count;

    // IO main loop
    sdk_fd_entry m_fds[SDK_MAX_FDS];
    int fd_count;

    // HTTP Client (lazy init)
    void* http_client;

    // State
    bool running;

    // History / Backfill
    int64_t history_start_ts;
    int64_t current_tick_time;  // Context for {date} substitution
    bool in_backfill_mode;
    int backfill_delay_ms;
};

// Internal helpers
int sdk_ipc_connect(plugin_ctx* ctx);
int sdk_ipc_send(plugin_ctx* ctx, const char* json_msg);
int sdk_ipc_recv(plugin_ctx* ctx, char* buf, size_t len);
bool sdk_ipc_check_data(plugin_ctx* ctx, int64_t ts);

#endif
