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
#include "memory.h"
#include "sdk_internal.h"

// Parse args: --socket <path> --id <id>
int sdk_create(plugin_ctx **ctx_out, int argc, char **argv)
{
    int ret = 0;
    plugin_ctx *ctx = NULL;

    if (!ctx_out) return -EINVAL;

    ctx = mem_alloc(sizeof(*ctx));
    if (!ctx) return -ENOMEM;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc)
        {
            size_t len = strlen(argv[i + 1]);
            ctx->socket_path = mem_alloc(len + 1);
            if (ctx->socket_path) strcpy(ctx->socket_path, argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc)
        {
            size_t len = strlen(argv[i + 1]);
            ctx->plugin_id = mem_alloc(len + 1);
            if (ctx->plugin_id) strcpy(ctx->plugin_id, argv[i + 1]);
            i++;
        }
    }

    if (!ctx->socket_path || !ctx->plugin_id)
    {
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

void sdk_destroy(plugin_ctx **ctx_ptr)
{
    if (!ctx_ptr || !*ctx_ptr) return;
    plugin_ctx *ctx = *ctx_ptr;

    if (ctx->ipc_fd >= 0)
    {
        close(ctx->ipc_fd);
    }

    mem_free(ctx->socket_path);
    mem_free(ctx->plugin_id);

    // Free ticker cron expressions (memory leak fix)
    for (int i = 0; i < ctx->ticker_count; i++)
    {
        if (ctx->tickers[i].is_cron)
        {
            mem_free(ctx->tickers[i].cron_expr);
        }
    }

    mem_free(ctx);
    *ctx_ptr = NULL;
}

// Internal: Connect to Core
int sdk_ipc_connect(plugin_ctx *ctx)
{
    int ret = 0;
    int fd = -1;

    if (ctx->connected) return 0;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -errno;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void) snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", ctx->socket_path);

    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    ctx->ipc_fd = fd;
    ctx->connected = true;

    // Handshake: Send ID
    char buf[256];
    (void) snprintf(buf, sizeof(buf), "{\"cmd\":\"hello\", \"id\":\"%s\"}", ctx->plugin_id);
    if (sdk_ipc_send(ctx, buf) < 0)
    {
        ret = -EIO;
        goto cleanup;
    }

    return 0;

cleanup:
    if (fd >= 0) close(fd);
    ctx->ipc_fd = -1;
    ctx->connected = false;
    return ret;
}

int sdk_run(plugin_ctx *ctx)
{
    if (sdk_ipc_connect(ctx) < 0)
    {
        // Fallback or error?
        // If we can't connect, can we still run tickers?
        // Probably not, as we can't report.
        return -1;
    }

    // --- History Configuration ---
    ctx->in_backfill_mode = false;
    ctx->backfill_delay_ms = 1000;  // Default: 1 second safe delay

    char *hist_val = NULL;
    if (sdk_get_config(ctx, "history_days", &hist_val) == 0 && hist_val)
    {
        long days = strtol(hist_val, NULL, 10);
        free(hist_val);

        if (days > 0)
        {
            ctx->history_start_ts = time(NULL) - ((time_t) days * 86400);
            ctx->in_backfill_mode = true;

            // Check for rate limit override
            char *rate_val = NULL;
            if (sdk_get_config(ctx, "history_rate_limit_ms", &rate_val) == 0 && rate_val)
            {
                long rate_ms = strtol(rate_val, NULL, 10);
                ctx->backfill_delay_ms = (int) rate_ms;
                if (ctx->backfill_delay_ms < 100) ctx->backfill_delay_ms = 100;  // Minimum safety
                free(rate_val);
            }
        }
    }

    ctx->running = true;

    // --- Backfill Loop ---
    if (ctx->in_backfill_mode)
    {
        int64_t now = time(NULL);
        int64_t iter = ctx->history_start_ts;

        // Determine step size. Use smallest ticker interval or default 1h.
        int64_t step = 3600;
        if (ctx->ticker_count > 0)
        {
            // Find smallest interval
            // TODO: Cron support? For now just use interval_sec if >0
            // Assuming at least one interval ticker for simplicity of MVP backfill
            for (int i = 0; i < ctx->ticker_count; i++)
            {
                if (!ctx->tickers[i].is_cron && ctx->tickers[i].interval_sec > 0)
                {
                    if (ctx->tickers[i].interval_sec < step) step = ctx->tickers[i].interval_sec;
                }
            }
        }

        sdk_log(ctx, SDK_LOG_INFO, "Starting history backfill from %ld (step %ld s)", iter, step);

        while (iter < now)
        {
            // Check Core DB
            if (!sdk_ipc_check_data(ctx, iter))
            {
                // Data missing, fetch it
                ctx->current_tick_time = iter;

                for (int i = 0; i < ctx->ticker_count; i++)
                {
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
    while (ctx->running)
    {
        // 1. Prepare poll fds
        // Size = 1 (IPC) + registered FDs
        struct pollfd fds[1 + SDK_MAX_FDS];
        int nfds = 0;

        // IPC
        fds[nfds].fd = ctx->ipc_fd;
        fds[nfds].events = POLLIN;
        nfds++;

        // User FDs
        for (int i = 0; i < ctx->fd_count; i++)
        {
            fds[nfds].fd = ctx->m_fds[i].fd;
            fds[nfds].events = POLLIN;  // Just read for now
            nfds++;
        }

        // 2. Calculate timeout (tickers)
        int timeout_ms = -1;  // Infinite
        int64_t now = time(NULL);
        int64_t next_event = -1;

        for (int i = 0; i < ctx->ticker_count; i++)
        {
            if (ctx->tickers[i].next_run < 0)
            {
                // Initialize next run
                ctx->tickers[i].next_run = now;  // Run immediately? Or align?
                // For interval, maybe run immediately or now + interval.
                // For cron, calculate next.
            }
            if (next_event == -1 || ctx->tickers[i].next_run < next_event)
            {
                next_event = ctx->tickers[i].next_run;
            }
        }

        if (next_event != -1)
        {
            int64_t diff = next_event - now;
            if (diff < 0) diff = 0;
            timeout_ms = (int) diff * 1000;
        }

        // 3. Poll
        int ret = poll(fds, nfds, timeout_ms);

        if (ret < 0)
        {
            if (errno == EINTR) continue;
            break;  // Error
        }

        // 4. Handle IO
        if (fds[0].revents & POLLIN)
        {
            // IPC Message - use buffered receive for proper framing
            char buf[SDK_IPC_BUFFER_SIZE];
            int recv_ret = sdk_ipc_recv(ctx, buf, sizeof(buf));
            if (recv_ret < 0)
            {
                // Core disconnected or error
                ctx->running = false;
                break;
            }
            // Handle message
            cJSON *json = cJSON_Parse(buf);
            if (json)
            {
                cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
                if (cmd && cmd->valuestring && strcmp(cmd->valuestring, "http_request") == 0)
                {
                    cJSON *method = cJSON_GetObjectItem(json, "method");
                    cJSON *path = cJSON_GetObjectItem(json, "path");
                    cJSON *query = cJSON_GetObjectItem(json, "query");

                    if (method && path)
                    {
                        // Find handler
                        for (int i = 0; i < ctx->endpoint_count; i++)
                        {
                            if (strcmp(ctx->endpoints[i].path, path->valuestring) == 0 &&
                                strcmp(ctx->endpoints[i].method, method->valuestring) == 0)
                            {
                                // Prepare Req/Resp
                                struct sdk_req req;
                                struct sdk_resp resp;
                                memset(&req, 0, sizeof(req));
                                memset(&resp, 0, sizeof(resp));

                                snprintf(req.method, sizeof(req.method), "%s", method->valuestring);
                                snprintf(req.path, sizeof(req.path), "%s", path->valuestring);
                                if (query && query->valuestring)
                                    snprintf(req.query, sizeof(req.query), "%s",
                                             query->valuestring);

                                resp.status = 200;

                                // Call Handler
                                ctx->endpoints[i].handler(ctx, &req, &resp);

                                // Send Response
                                cJSON *resp_json = cJSON_CreateObject();
                                cJSON_AddStringToObject(resp_json, "cmd", "http_response");
                                cJSON_AddNumberToObject(resp_json, "status", (double) resp.status);
                                cJSON_AddStringToObject(resp_json, "body", resp.body);

                                // Tag with request ID provided by core?
                                // MVP: Core blocks on plugin request, so simple response is fine?
                                // Actually, Core likely tracks connection. Simple response works if
                                // Core is waiting. Or we might need a request ID. Assume Core waits
                                // for next message on this conn.

                                char *str = cJSON_PrintUnformatted(resp_json);
                                if (str)
                                {
                                    sdk_ipc_send(ctx, str);
                                    free(str);
                                }
                                cJSON_Delete(resp_json);
                                break;
                            }
                        }
                    }
                }
                else if (cmd && cmd->valuestring && strcmp(cmd->valuestring, "fetch_now") == 0)
                {
                    // Server requesting immediate fetch
                    sdk_log(ctx, SDK_LOG_INFO, "Received fetch_now command from server");

                    // Trigger all tickers immediately
                    int64_t now = time(NULL);
                    for (int i = 0; i < ctx->ticker_count; i++)
                    {
                        ctx->tickers[i].handler(ctx, now);
                        // Reset next run to maintain schedule
                        ctx->tickers[i].next_run = now + ctx->tickers[i].interval_sec;
                    }
                }
                cJSON_Delete(json);
            }
        }

        // User FDs
        int current_fd_idx = 0;
        for (int i = 1; i < nfds; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                // Find handler
                if (current_fd_idx < ctx->fd_count)
                {
                    ctx->m_fds[current_fd_idx].handler(ctx, fds[i].fd);
                }
            }
            current_fd_idx++;
        }

        // 5. Handle Tickers
        now = time(NULL);
        for (int i = 0; i < ctx->ticker_count; i++)
        {
            if (now >= ctx->tickers[i].next_run)
            {
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
