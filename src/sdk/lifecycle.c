#include <errno.h>
#include <heimwatt_sdk.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "sdk_internal.h"

// Parse args: --socket <path> --id <id>
int sdk_create(plugin_ctx** ctx_out, int argc, char** argv) {
    if (!ctx_out) return -1;

    plugin_ctx* ctx = calloc(1, sizeof(plugin_ctx));
    if (!ctx) return -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            ctx->socket_path = strdup(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            ctx->plugin_id = strdup(argv[i + 1]);
            i++;
        }
    }

    if (!ctx->socket_path || !ctx->plugin_id) {
        // Missing required args
        free(ctx->socket_path);
        free(ctx->plugin_id);
        free(ctx);
        return -1;
    }

    ctx->ipc_fd = -1;
    ctx->running = false;
    *ctx_out = ctx;
    return 0;
}

void sdk_destroy(plugin_ctx** ctx_ptr) {
    if (!ctx_ptr || !*ctx_ptr) return;
    plugin_ctx* ctx = *ctx_ptr;

    if (ctx->ipc_fd >= 0) {
        close(ctx->ipc_fd);
    }

    free(ctx->socket_path);
    free(ctx->plugin_id);

    // Free internal structures if needed (tickers etc)

    free(ctx);
    *ctx_ptr = NULL;
}

// Internal: Connect to Core
int sdk_ipc_connect(plugin_ctx* ctx) {
    if (ctx->connected) return 0;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ctx->socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    ctx->ipc_fd = fd;
    ctx->connected = true;

    // Handshake: Send ID
    // TODO: implement handshake protocol

    return 0;
}

int sdk_run(plugin_ctx* ctx) {
    if (sdk_ipc_connect(ctx) < 0) {
        // Fallback or error?
        // If we can't connect, can we still run tickers?
        // Probably not, as we can't report.
        return -1;
    }

    // --- History Configuration ---
    ctx->in_backfill_mode = false;
    ctx->backfill_delay_ms = 1000;  // Default: 1 second safe delay

    char* hist_val = NULL;
    if (sdk_get_config(ctx, "history_days", &hist_val) == 0 && hist_val) {
        int days = atoi(hist_val);
        free(hist_val);

        if (days > 0) {
            ctx->history_start_ts = time(NULL) - (days * 86400);
            ctx->in_backfill_mode = true;

            // Check for rate limit override
            char* rate_val = NULL;
            if (sdk_get_config(ctx, "history_rate_limit_ms", &rate_val) == 0 && rate_val) {
                ctx->backfill_delay_ms = atoi(rate_val);
                if (ctx->backfill_delay_ms < 100) ctx->backfill_delay_ms = 100;  // Minimum safety
                free(rate_val);
            }
        }
    }

    ctx->running = true;

    // --- Backfill Loop ---
    if (ctx->in_backfill_mode) {
        int64_t now = time(NULL);
        int64_t iter = ctx->history_start_ts;

        // Determine step size. Use smallest ticker interval or default 1h.
        int64_t step = 3600;
        if (ctx->ticker_count > 0) {
            // Find smallest interval
            // TODO: Cron support? For now just use interval_sec if >0
            // Assuming at least one interval ticker for simplicity of MVP backfill
            for (int i = 0; i < ctx->ticker_count; i++) {
                if (!ctx->tickers[i].is_cron && ctx->tickers[i].interval_sec > 0) {
                    if (ctx->tickers[i].interval_sec < step) step = ctx->tickers[i].interval_sec;
                }
            }
        }

        sdk_log(ctx, SDK_LOG_INFO, "Starting history backfill from %ld (step %ld s)", iter, step);

        while (iter < now) {
            // Check Core DB
            if (!sdk_ipc_check_data(ctx, iter)) {
                // Data missing, fetch it
                ctx->current_tick_time = iter;

                for (int i = 0; i < ctx->ticker_count; i++) {
                    ctx->tickers[i].handler(ctx, iter);
                }

                // Rate Limit
                usleep(ctx->backfill_delay_ms * 1000);
            }

            iter += step;
        }

        sdk_log(ctx, SDK_LOG_INFO, "Backfill complete. Entering realtime mode.");
        ctx->in_backfill_mode = false;
        ctx->current_tick_time = 0;
    }

    // Main loop
    while (ctx->running) {
        // 1. Prepare poll fds
        // Size = 1 (IPC) + registered FDs
        struct pollfd fds[1 + SDK_MAX_FDS];
        int nfds = 0;

        // IPC
        fds[nfds].fd = ctx->ipc_fd;
        fds[nfds].events = POLLIN;
        nfds++;

        // User FDs
        for (int i = 0; i < ctx->fd_count; i++) {
            fds[nfds].fd = ctx->m_fds[i].fd;
            fds[nfds].events = POLLIN;  // Just read for now
            nfds++;
        }

        // 2. Calculate timeout (tickers)
        int timeout_ms = -1;  // Infinite
        int64_t now = time(NULL);
        int64_t next_event = -1;

        for (int i = 0; i < ctx->ticker_count; i++) {
            if (ctx->tickers[i].next_run < 0) {
                // Initialize next run
                ctx->tickers[i].next_run = now;  // Run immediately? Or align?
                // For interval, maybe run immediately or now + interval.
                // For cron, calculate next.
            }
            if (next_event == -1 || ctx->tickers[i].next_run < next_event) {
                next_event = ctx->tickers[i].next_run;
            }
        }

        if (next_event != -1) {
            int64_t diff = next_event - now;
            if (diff < 0) diff = 0;
            timeout_ms = (int)diff * 1000;
        }

        // 3. Poll
        int ret = poll(fds, nfds, timeout_ms);

        if (ret < 0) {
            if (errno == EINTR) continue;
            break;  // Error
        }

        // 4. Handle IO
        if (fds[0].revents & POLLIN) {
            // IPC Message
            char buf[SDK_IPC_BUFFER_SIZE];
            ssize_t n = read(ctx->ipc_fd, buf, sizeof(buf) - 1);
            if (n <= 0) {
                // Core disconnected
                ctx->running = false;
                break;
            }
            buf[n] = 0;
            // Handle message (TODO)
        }

        // User FDs
        int current_fd_idx = 0;
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                // Find handler
                if (current_fd_idx < ctx->fd_count) {
                    ctx->m_fds[current_fd_idx].handler(ctx, fds[i].fd);
                }
            }
            current_fd_idx++;
        }

        // 5. Handle Tickers
        now = time(NULL);
        for (int i = 0; i < ctx->ticker_count; i++) {
            if (now >= ctx->tickers[i].next_run) {
                // Run handler
                ctx->tickers[i].handler(ctx, now);  // Pass timestamp?

                // Reschedule
                // Simple interval
                ctx->tickers[i].next_run = now + ctx->tickers[i].interval_sec;
                // TODO: Cron logic
            }
        }
    }

    return 0;
}
