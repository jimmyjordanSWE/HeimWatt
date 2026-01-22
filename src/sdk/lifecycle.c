#include "memory.h"
#include "sdk_internal.h"

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

#include "libs/cJSON.h"

// Parse args: --socket <path> --id <id>
int sdk_create(plugin_ctx** ctx_out, int argc, char** argv) {
    int ret = 0;
    plugin_ctx* ctx = NULL;

    if (!ctx_out)
        return -EINVAL;

    ctx = mem_alloc(sizeof(*ctx));
    if (!ctx)
        return -ENOMEM;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            size_t len = strlen(argv[i + 1]);
            ctx->socket_path = mem_alloc(len + 1);
            if (ctx->socket_path)
                memcpy(ctx->socket_path, argv[i + 1], len + 1);
            i++;
        } else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            size_t len = strlen(argv[i + 1]);
            ctx->plugin_id = mem_alloc(len + 1);
            if (ctx->plugin_id)
                memcpy(ctx->plugin_id, argv[i + 1], len + 1);
            i++;
        }
    }

    if (!ctx->socket_path || !ctx->plugin_id) {
        ret = -EINVAL;
        goto cleanup;
    }

    ctx->ipc_fd = -1;
    ctx->running = false;
    *ctx_out = ctx;
    return 0;

cleanup:
    sdk_destroy(&ctx);
    return ret;
}

void sdk_destroy(plugin_ctx** ctx_ptr) {
    if (!ctx_ptr || !*ctx_ptr)
        return;
    plugin_ctx* ctx = *ctx_ptr;

    if (ctx->ipc_fd >= 0) {
        close(ctx->ipc_fd);
    }

    mem_free(ctx->socket_path);
    mem_free(ctx->plugin_id);

    // Free ticker cron expressions (memory leak fix)
    for (int i = 0; i < ctx->ticker_count; i++) {
        if (ctx->tickers[i].is_cron) {
            mem_free(ctx->tickers[i].cron_expr);
        }
    }

    mem_free(ctx);
    *ctx_ptr = NULL;
}

// Internal: Connect to Core
int sdk_ipc_connect(plugin_ctx* ctx) {
    int ret = 0;
    int fd = -1;

    if (ctx->connected)
        return 0;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void) snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", ctx->socket_path);

    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        ret = -errno;
        goto cleanup;
    }

    ctx->ipc_fd = fd;
    ctx->connected = true;

    // Handshake: Send ID
    char buf[256];
    (void) snprintf(buf, sizeof(buf), "{\"cmd\":\"hello\", \"id\":\"%s\"}", ctx->plugin_id);
    if (sdk_ipc_send(ctx, buf) < 0) {
        ret = -EIO;
        goto cleanup;
    }

    return 0;

cleanup:
    if (fd >= 0)
        close(fd);
    ctx->ipc_fd = -1;
    ctx->connected = false;
    return ret;
}

// Helper forward decls
static void sdk_run_backfill(plugin_ctx* ctx, sdk_eventloop* loop);
static void sdk_handle_ipc_event(void* user_ctx, int fd, int events);
static void sdk_handle_user_fd(void* user_ctx, int fd, int events);
static int64_t sdk_cron_next_run_wrapper(void* user_ctx, int64_t now);

// Bridge struct for passing context + ticker info to callbacks if needed
// Actually, for simple tickers, user_ctx is plugin_ctx.
// But we need to know WHICH ticker handler to call if we register them individually.
// The eventloop ticker callback is `void (*)(void *ctx, int64_t now)`.
// The SDK ticker handler is `void (*)(plugin_ctx *ctx, int64_t now)`.
// So we can pass plugin_ctx directly.
// But wait, if we have multiple tickers, how do we distinguish?
// We need to register EACH ticker separately in the eventloop.
// We need a wrapper context per ticker if we want to call the specific underlying handler?
// No, `sdk_register_ticker` takes a handler.
// `plugin_ctx` stores `sdk_ticker_entry`.
// We can use the entry itself as context?
// `sdk_ticker_entry` has `handler`.
// So we need a bridge function:
// void bridge(void *entry_ptr, int64_t now) {
//    sdk_ticker_entry *e = entry_ptr;
//    // We need plugin_ctx too?
//    // The handler signature is: `typedef void (*sdk_tick_handler)(plugin_ctx *ctx, time_t dt);`
//    // So yes, we need plugin_ctx.
// }
// So we need a wrapper struct or similar.
// Or we just modify sdk_ticker_entry to include plugin_ctx backpointer?
// Or we allocate a wrapper at runtime in sdk_run?

typedef struct {
    plugin_ctx* ctx;
    sdk_ticker_entry* entry;
} ticker_wrapper;

static void sdk_ticker_wrapper_fn(void* user_ctx, int64_t now) {
    ticker_wrapper* w = user_ctx;
    if (w && w->entry && w->entry->handler) {
        w->ctx->current_tick_time = now;  // Update context time
        w->entry->handler(w->ctx, now);
    }
}

// Cron support
// sdk_next_run_fn is (void *ctx, int64_t now) -> int64_t
static int64_t sdk_cron_next_run_wrapper(void* user_ctx, int64_t now) {
    ticker_wrapper* w = user_ctx;
    (void) w;
    // TODO: implement actual cron parsing/calculation using w->entry->cron_expr
    // For MVP/Safety, just return now + 60s or something if not implemented
    // Note: The original code didn't fully implement cron logic in the loop either,
    // it just had a TODO.
    // "TODO: Cron logic" was in the original file.
    return now + 60;
}

int sdk_run(plugin_ctx* ctx) {
    if (sdk_ipc_connect(ctx) < 0) {
        return -1;
    }

    // --- History Configuration ---
    ctx->in_backfill_mode = false;
    ctx->backfill_delay_ms = 1000;

    char* hist_val = NULL;
    if (sdk_get_config(ctx, "history_days", &hist_val) == 0 && hist_val) {
        long days = strtol(hist_val, NULL, 10);
        free(hist_val);

        if (days > 0) {
            ctx->history_start_ts = time(NULL) - ((time_t) days * 86400);
            ctx->in_backfill_mode = true;

            char* rate_val = NULL;
            if (sdk_get_config(ctx, "history_rate_limit_ms", &rate_val) == 0 && rate_val) {
                long rate_ms = strtol(rate_val, NULL, 10);
                ctx->backfill_delay_ms = (int) rate_ms;
                if (ctx->backfill_delay_ms < 100)
                    ctx->backfill_delay_ms = 100;
                free(rate_val);
            }
        }
    }

    ctx->running = true;

    // Create Event Loop
    ctx->loop = sdk_eventloop_create();
    if (!ctx->loop)
        return -ENOMEM;

    // Register IPC
    sdk_eventloop_add_fd(ctx->loop, ctx->ipc_fd, POLLIN, sdk_handle_ipc_event, ctx);

    // Register User FDs
    // We need wrappers for these too if we assume 1:1, but the original code
    // just iterated m_fds in the loop.
    // Here we can register each individually.
    // Wrapper needed?
    // sdk_handle_user_fd below needs to know which handler to call.
    // We can use a wrapper similar to tickers.
    // Or we can just pass the sdk_fd_entry pointer if it's stable.
    // Yes, ctx->m_fds is a stable array inside ctx.
    for (int i = 0; i < ctx->fd_count; i++) {
        // We need to pass both ctx (for API access) and the entry (for handler).
        // Let's alloc a small wrapper or just assume we can derive it.
        // Actually, for FDs, standard callback is (ctx, fd, events).
        // The user handler is (plugin_ctx, fd).
        // So passing plugin_ctx is enough! The handler is in the array but we don't know WHICH one.
        // Wait, sdk_eventloop_add_fd takes a callback.
        // We can define a unique callback for each? No.
        // We can pass a struct { plugin_ctx*, sdk_fd_entry* } as context.
        // Since we are inside sdk_run, we can allocate an array of wrappers on stack?
        // No, sdk_run returns? No, blocking.
        // So we can allocate wrappers on stack if they outlive the run?
        // Yes, sdk_eventloop_run blocks.
        // But the wrappers need to be stable.
    }

    // Allocate wrappers for FDs
    typedef struct {
        plugin_ctx* c;
        sdk_io_handler h;
    } fd_wrap;
    fd_wrap* fd_wraps = calloc(ctx->fd_count, sizeof(fd_wrap));
    for (int i = 0; i < ctx->fd_count; i++) {
        fd_wraps[i].c = ctx;
        fd_wraps[i].h = ctx->m_fds[i].handler;
        sdk_eventloop_add_fd(ctx->loop, ctx->m_fds[i].fd, POLLIN, sdk_handle_user_fd, &fd_wraps[i]);
    }

    // Register Tickers
    ticker_wrapper* tick_wraps = calloc(ctx->ticker_count, sizeof(ticker_wrapper));
    for (int i = 0; i < ctx->ticker_count; i++) {
        tick_wraps[i].ctx = ctx;
        tick_wraps[i].entry = &ctx->tickers[i];  // Stable pointer into ctx array

        if (ctx->tickers[i].is_cron) {
            sdk_eventloop_add_scheduled_task(ctx->loop, sdk_cron_next_run_wrapper,
                                             sdk_ticker_wrapper_fn, &tick_wraps[i]);
        } else {
            // Basic interval
            // Note: interval_sec must be > 0.
            int interval = (int) ctx->tickers[i].interval_sec;
            if (interval <= 0)
                interval = 1;  // Safety
            sdk_eventloop_add_ticker(ctx->loop, interval, sdk_ticker_wrapper_fn, &tick_wraps[i]);
        }
    }

    // Backfill
    if (ctx->in_backfill_mode) {
        sdk_run_backfill(ctx, ctx->loop);
    }

    // Run Loop
    sdk_eventloop_run(ctx->loop);

    // Cleanup
    free(fd_wraps);
    free(tick_wraps);
    sdk_eventloop_destroy(&ctx->loop);

    return 0;
}

static void sdk_run_backfill(plugin_ctx* ctx, sdk_eventloop* loop) {
    (void) loop;
    int64_t now = time(NULL);
    int64_t iter = ctx->history_start_ts;

    // Smallest step
    int64_t step = 3600;
    if (ctx->ticker_count > 0) {
        for (int i = 0; i < ctx->ticker_count; i++) {
            if (!ctx->tickers[i].is_cron && ctx->tickers[i].interval_sec > 0) {
                if (ctx->tickers[i].interval_sec < step)
                    step = ctx->tickers[i].interval_sec;
            }
        }
    }

    sdk_log(ctx, SDK_LOG_INFO, "Starting history backfill from %ld (step %ld s)", iter, step);

    while (iter < now && ctx->running) {
        if (!sdk_ipc_check_data(ctx, iter)) {
            ctx->current_tick_time = iter;
            for (int i = 0; i < ctx->ticker_count; i++) {
                if (ctx->tickers[i].handler) {
                    ctx->tickers[i].handler(ctx, iter);
                }
            }
            usleep(ctx->backfill_delay_ms * 1000);
        }
        iter += step;
    }

    sdk_log(ctx, SDK_LOG_INFO, "Backfill complete. Entering realtime mode.");
    ctx->in_backfill_mode = false;
    ctx->current_tick_time = 0;
}

static void sdk_handle_user_fd(void* user_ctx, int fd, int events) {
    (void) events;  // Assume POLLIN for now
    // Reconstruct wrapper
    // We defined fd_wrap locally in sdk_run. It's safe because sdk_run handles the loop.
    typedef struct {
        plugin_ctx* c;
        sdk_io_handler h;
    } fd_wrap;
    fd_wrap* w = user_ctx;
    if (w && w->h) {
        w->h(w->c, fd);
    }
}

static void sdk_handle_ipc_event(void* user_ctx, int fd, int events) {
    plugin_ctx* ctx = user_ctx;
    (void) fd;
    if (!ctx)
        return;

    if (events & POLLIN) {
        char buf[SDK_IPC_BUFFER_SIZE];
        int recv_ret = sdk_ipc_recv(ctx, buf, sizeof(buf));
        if (recv_ret < 0) {
            ctx->running = false;
            sdk_eventloop_stop(ctx->loop);
            return;
        }

        cJSON* json = cJSON_Parse(buf);
        if (json) {
            cJSON* cmd = cJSON_GetObjectItem(json, "cmd");

            // Dispatch
            if (cmd && cmd->valuestring && strcmp(cmd->valuestring, "http_request") == 0) {
                cJSON* method = cJSON_GetObjectItem(json, "method");
                cJSON* path = cJSON_GetObjectItem(json, "path");
                cJSON* query = cJSON_GetObjectItem(json, "query");

                if (method && path) {
                    for (int i = 0; i < ctx->endpoint_count; i++) {
                        if (strcmp(ctx->endpoints[i].path, path->valuestring) == 0 &&
                            strcmp(ctx->endpoints[i].method, method->valuestring) == 0) {
                            struct sdk_req req;
                            struct sdk_resp resp;
                            memset(&req, 0, sizeof(req));
                            memset(&resp, 0, sizeof(resp));

                            snprintf(req.method, sizeof(req.method), "%s", method->valuestring);
                            snprintf(req.path, sizeof(req.path), "%s", path->valuestring);
                            if (query && query->valuestring)
                                snprintf(req.query, sizeof(req.query), "%s", query->valuestring);

                            resp.status = 200;
                            ctx->endpoints[i].handler(ctx, &req, &resp);

                            cJSON* resp_json = cJSON_CreateObject();
                            cJSON_AddStringToObject(resp_json, "cmd", "http_response");
                            cJSON_AddNumberToObject(resp_json, "status", (double) resp.status);
                            cJSON_AddStringToObject(resp_json, "body", resp.body);

                            char* str = cJSON_PrintUnformatted(resp_json);
                            if (str) {
                                sdk_ipc_send(ctx, str);
                                free(str);
                            }
                            cJSON_Delete(resp_json);
                            break;
                        }
                    }
                }
            } else if (cmd && cmd->valuestring && strcmp(cmd->valuestring, "fetch_now") == 0) {
                sdk_log(ctx, SDK_LOG_INFO, "Received fetch_now command from server");
                int64_t now = time(NULL);
                for (int i = 0; i < ctx->ticker_count; i++) {
                    if (ctx->tickers[i].handler) {
                        ctx->tickers[i].handler(ctx, now);
                    }
                }
            }

            cJSON_Delete(json);
        }
    }
}
