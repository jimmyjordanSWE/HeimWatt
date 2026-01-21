#include "server.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

#include "core/config.h"
#include "core/ipc.h"
#include "core/plugin_mgr.h"
#include "db.h"
#include "ipc_handlers.h"
#include "libs/cJSON.h"
#include "log.h"
#include "log_ring.h"
#include "log_structured.h"
#include "memory.h"
#include "net/http_server.h"
#include "server_internal.h"
#include "util/thread_pool.h"

// Global for signal handling if needed, but we pass ctx via args
static volatile int g_shutdown = 0;

heimwatt_ctx *heimwatt_create(void)
{
    heimwatt_ctx *ctx = mem_alloc(sizeof(*ctx));
    if (ctx)
    {
        pthread_mutex_init(&ctx->conn_lock, NULL);
        ctx->pool = thread_pool_create(4);  // 4 workers
    }
    return ctx;
}

// --- Admin API Handlers ---

static void api_status(heimwatt_ctx *ctx, http_response *resp)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "running");
    cJSON_AddNumberToObject(json, "uptime", (double) clock() / CLOCKS_PER_SEC);  // Simple uptime
    cJSON_AddNumberToObject(json, "connections", ctx->conn_count);
    cJSON_AddNumberToObject(json, "plugins", plugin_mgr_count(ctx->plugins));

    char *str = cJSON_PrintUnformatted(json);
    http_response_set_json(resp, str);
    mem_free(str);
    cJSON_Delete(json);
}

// Helper struct for callback
struct plugins_list_ctx
{
    cJSON *arr;
    heimwatt_ctx *ctx;
};

static void plugins_list_iter(const plugin_handle *h, void *u)
{
    struct plugins_list_ctx *ic = (struct plugins_list_ctx *) u;
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "id", plugin_handle_id(h));
    cJSON_AddStringToObject(p, "resource", plugin_handle_resource(h));
    cJSON_AddNumberToObject(p, "pid", (double) plugin_handle_pid(h));
    cJSON_AddStringToObject(p, "type", plugin_handle_type(h) == PLUGIN_TYPE_IN ? "in" : "out");

    plugin_state s = plugin_handle_state(h);
    const char *s_str = "unknown";
    if (s == PLUGIN_STATE_STOPPED)
        s_str = "stopped";
    else if (s == PLUGIN_STATE_STARTING)
        s_str = "starting";
    else if (s == PLUGIN_STATE_RUNNING)
        s_str = "running";
    else if (s == PLUGIN_STATE_STOPPING)
        s_str = "stopping";
    else if (s == PLUGIN_STATE_RESTARTING)
        s_str = "restarting";
    else if (s == PLUGIN_STATE_FAILED)
        s_str = "failed";
    cJSON_AddStringToObject(p, "state", s_str);

    // Add schedule info
    cJSON_AddNumberToObject(p, "interval", plugin_handle_interval(h));
    cJSON_AddNumberToObject(p, "last_run", (double) plugin_handle_last_run(h));

    // Add endpoint info (if any)
    const char *id = plugin_handle_id(h);
    if (id)
    {
        for (int i = 0; i < ic->ctx->registry_count; i++)
        {
            if (strcmp(ic->ctx->registry[i].plugin_id, id) == 0)
            {
                cJSON_AddStringToObject(p, "endpoint", ic->ctx->registry[i].path);
                break;  // Only first endpoint
            }
        }
    }

    cJSON_AddItemToArray(ic->arr, p);
}

static void api_plugins_list(heimwatt_ctx *ctx, http_response *resp)
{
    cJSON *arr = cJSON_CreateArray();

    struct plugins_list_ctx iter_ctx = {arr, ctx};
    plugin_mgr_foreach(ctx->plugins, plugins_list_iter, &iter_ctx);

    char *str = cJSON_PrintUnformatted(arr);
    http_response_set_json(resp, str);
    mem_free(str);
    cJSON_Delete(arr);
}

static void api_plugins_control(heimwatt_ctx *ctx, const char *id, const char *action,
                                http_response *resp)
{
    int ret = -1;
    if (strcmp(action, "start") == 0)
        ret = plugin_mgr_start(ctx->plugins, id);
    else if (strcmp(action, "stop") == 0)
        ret = plugin_mgr_stop(ctx->plugins, id);
    else if (strcmp(action, "restart") == 0)
        ret = plugin_mgr_restart(ctx->plugins, id);
    else
    {
        http_response_set_status(resp, 400);
        http_response_set_json(resp, "{\"error\": \"Invalid action\"}");
        return;
    }

    if (ret == 0)
    {
        http_response_set_json(resp, "{\"success\": true}");
    }
    else
    {
        http_response_set_status(resp, 500);
        http_response_set_json(resp, "{\"error\": \"Action failed\"}");
    }
}

static int handle_api_request(heimwatt_ctx *ctx, const http_request *req, http_response *resp)
{
    if (strcmp(req->path, "/api/status") == 0)
    {
        api_status(ctx, resp);
        return 0;
    }
    else if (strcmp(req->path, "/api/plugins") == 0)
    {
        api_plugins_list(ctx, resp);
        return 0;
    }
    else if (strncmp(req->path, "/api/plugins/", 13) == 0)
    {
        // /api/plugins/:id/:action
        // Format: /api/plugins/se.smhi.weather/start
        // Find next slash
        const char *id_start = req->path + 13;
        const char *slash = strchr(id_start, '/');
        if (slash)
        {
            char id[64];
            int id_len = slash - id_start;
            if (id_len < 64)
            {
                memcpy(id, id_start, id_len);
                id[id_len] = '\0';
                const char *action = slash + 1;
                api_plugins_control(ctx, id, action, resp);
                return 0;
            }
        }
    }
    else if (strcmp(req->path, "/api/logs") == 0)
    {
        char *json = log_ring_to_json(100);
        if (json)
        {
            http_response_set_json(resp, json);
            free(json);
        }
        else
        {
            http_response_set_status(resp, 500);
            http_response_set_json(resp, "{\"error\": \"Internal Error\"}");
        }
        return 0;
    }

    return -1;  // Not an API request we handled
}

// Async HTTP handler - returns 1 if pending, 0 if completed synchronously
static int http_async_handler(const http_request *req, http_response *resp, const char *request_id,
                              void *arg)
{
    heimwatt_ctx *ctx = (heimwatt_ctx *) arg;

    // 0. Intercept Admin API
    if (strncmp(req->path, "/api/", 5) == 0)
    {
        if (handle_api_request(ctx, req, resp) == 0)
        {
            return 0;  // Handled synchronously by core
        }
        // If not handled by core, fall through to see if a plugin registered this /api/ path
    }

    // 1. Find plugin for path
    ipc_conn *target = NULL;
    char target_plugin_id[64] = {0};

    pthread_mutex_lock(&ctx->conn_lock);
    for (int i = 0; i < ctx->registry_count; i++)
    {
        if (strcmp(ctx->registry[i].path, req->path) == 0 &&
            strcmp(ctx->registry[i].method, req->method) == 0)
        {
            // Find connection by ID
            for (int j = 0; j < ctx->conn_count; j++)
            {
                const char *pid = ipc_conn_plugin_id(ctx->conns[j]);
                if (pid && strcmp(pid, ctx->registry[i].plugin_id) == 0)
                {
                    target = ctx->conns[j];
                    snprintf(target_plugin_id, sizeof(target_plugin_id), "%s", pid);
                    break;
                }
            }
        }
        if (target) break;
    }
    pthread_mutex_unlock(&ctx->conn_lock);

    if (!target)
    {
        // No matching endpoint - return 404 synchronously
        http_response_set_status(resp, 404);
        http_response_set_json(resp, "{\"error\": \"Not Found\"}");
        return 0;  // Completed synchronously
    }

    // 2. Send IPC Request with request_id for correlation
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "cmd", "http_request");
    cJSON_AddStringToObject(json, "request_id", request_id);
    cJSON_AddStringToObject(json, "method", req->method);
    cJSON_AddStringToObject(json, "path", req->path);
    cJSON_AddStringToObject(json, "query", req->query);
    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (str)
    {
        pthread_mutex_lock(&ctx->conn_lock);
        ipc_conn_send(target, str, strlen(str));
        pthread_mutex_unlock(&ctx->conn_lock);
        mem_free(str);

        log_debug("[HTTP] Request forwarded to %s (id=%.8s...)", target_plugin_id, request_id);
        return 1;  // Pending - wait for IPC response
    }

    // Failed to serialize
    http_response_set_status(resp, 500);
    http_response_set_json(resp, "{\"error\": \"Internal Error\"}");
    return 0;
}

static void *http_thread_func(void *arg)
{
    /* Block SIGINT/SIGTERM in this thread so main thread receives them */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    heimwatt_ctx *ctx = (heimwatt_ctx *) arg;
    http_server_run(ctx->http);
    return NULL;
}

static void log_ring_callback(log_Event *ev)
{
    log_entry entry;
    // Use wall clock time
    entry.timestamp = (int64_t) time(NULL);
    entry.level = ev->level;
    entry.category[0] = '\0';
    entry.event[0] = '\0';
    entry.message[0] = '\0';

    // Format message
    vsnprintf(entry.message, sizeof(entry.message), ev->fmt, ev->ap);

    // Try to parse [CATEGORY]
    if (entry.message[0] == '[')
    {
        char *end = strchr(entry.message, ']');
        if (end)
        {
            size_t len = end - entry.message - 1;
            if (len < sizeof(entry.category))
            {
                memcpy(entry.category, entry.message + 1, len);
                entry.category[len] = '\0';
            }
        }
    }
    else
    {
        strncpy(entry.category, "general", sizeof(entry.category) - 1);
    }

    log_ring_push(&entry);
}

int heimwatt_init(heimwatt_ctx *ctx, const char *base_path)
{
    int ret = 0;
    if (!ctx) return -EINVAL;

    // Init HTTP Server
    if (http_server_create(&ctx->http, 8080) < 0)
    {
        log_error("[INIT] Failed to create HTTP server");
        return -1;
    }
    http_server_set_async_handler(ctx->http, http_async_handler, ctx);

    const char *path = base_path ? base_path : "data/default";

    // Initialize logging
    log_set_level(LOG_INFO);

    // Initialize Ring Buffer
    log_ring_init(100);
    log_add_callback(log_ring_callback, NULL, LOG_TRACE);

    // Create log file
    char log_path[256];
    (void) snprintf(log_path, sizeof(log_path), "%s/heimwatt.log", path);
    ctx->log_file = fopen(log_path, "a");
    if (ctx->log_file)
    {
        log_add_fp(ctx->log_file, LOG_INFO);
        log_info("[INIT] Log file opened: %s", log_path);
    }

    log_info("[INIT] HeimWatt Core starting...");
    log_info("[INIT] Storage path: %s", path);

    // 0.5 Load Config
    config *cfg = config_create();
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "config/heimwatt.json");

    // Load defaults first
    if (config_load(cfg, config_path) == 0)
    {
        const char *loc_name = config_get_loc_name(cfg);
        log_info("[INIT] Config loaded. CSV Interval: %ds, Location: %s",
                 config_get_csv_interval(cfg), loc_name);

        ctx->lat = config_get_lat(cfg);
        ctx->lon = config_get_lon(cfg);
        snprintf(ctx->area, sizeof(ctx->area), "%s", config_get_area(cfg));
    }
    else
    {
        log_warn("[INIT] Failed to load config, using defaults");
    }

    // 1. Open DB
    ret = db_open(&ctx->db, cfg);
    if (ret < 0)
    {
        log_error("[INIT] Failed to open DB: %s", strerror(-ret));
        config_destroy(&cfg);
        goto cleanup;
    }
    db_set_interval(ctx->db, config_get_csv_interval(cfg));
    config_destroy(&cfg);
    log_info("[INIT] Database opened: %s", path);

    // 1.5 Initialize Plugin Manager
    const char *socket_path = "/tmp/heimwatt.sock";
    ret = plugin_mgr_init(&ctx->plugins, "plugins", socket_path);
    if (ret < 0)
    {
        log_error("[INIT] Failed to init plugin manager");
        goto cleanup;
    }

    // 1.6 Scan and start plugins
    plugin_mgr_scan(ctx->plugins);
    (void) plugin_mgr_validate(ctx->plugins);

    // 1.7 Check dependencies and bootstrap if DB is empty
    bool needs_bootstrap = db_is_empty(ctx->db);
    if (needs_bootstrap)
    {
        log_info("[INIT] Empty database detected - will attempt to bootstrap required data");
    }

    // Reports dependencies and triggers bootstrap fetch if needs_bootstrap is true
    const char *missing_path = "logs/missing_providers.log";
    (void) plugin_mgr_validate_dependencies(ctx->plugins, missing_path, needs_bootstrap);

    // 2. Start IPC (before plugins so they can connect)
    ret = ipc_server_init(&ctx->ipc, socket_path);
    if (ret < 0)
    {
        log_error("[INIT] Failed to init IPC at %s: %s", socket_path, strerror(-ret));
        goto cleanup;
    }
    log_info("[INIT] IPC server listening: %s", socket_path);

    // 3. Start plugins (they will connect to IPC)
    plugin_mgr_start_all(ctx->plugins);

    log_info("[INIT] Initialization complete");
    return 0;

cleanup:
    plugin_mgr_destroy(&ctx->plugins);
    db_close(&ctx->db);
    return ret;
}

void heimwatt_run(heimwatt_ctx *ctx) { heimwatt_run_with_shutdown_flag(ctx, NULL); }

// Sentinel for signalfd epoll event
static char SIGNAL_FD_TAG;

void heimwatt_run_with_shutdown_flag(heimwatt_ctx *ctx, const volatile sig_atomic_t *shutdown_flag)
{
    if (!ctx) return;
    atomic_store(&ctx->running, 1);

    // 1. Setup signalfd for SIGINT/SIGTERM
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    // Signalfd must be non-blocking for epoll
    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd < 0)
    {
        log_fatal("[CORE] Failed to create signalfd: %s", strerror(errno));
        atomic_store(&ctx->running, 0);
        return;
    }

    // 2. Add signalfd to IPC epoll loop
    int epoll_fd = ipc_server_get_epoll_fd(ctx->ipc);
    if (epoll_fd < 0)
    {
        log_fatal("[CORE] Failed to get internal epoll fd");
        close(sfd);
        atomic_store(&ctx->running, 0);
        return;
    }

    struct epoll_event sig_ev;
    memset(&sig_ev, 0, sizeof(sig_ev));
    sig_ev.events = EPOLLIN;
    sig_ev.data.ptr = &SIGNAL_FD_TAG;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sfd, &sig_ev) < 0)
    {
        log_fatal("[CORE] Failed to add signalfd to epoll: %s", strerror(errno));
        close(sfd);
        atomic_store(&ctx->running, 0);
        return;
    }

    enum
    {
        MAX_EVENTS = 64
    };
    struct epoll_event events[MAX_EVENTS];

    log_info("[CORE] Main loop started (epoll-based, max 32 plugins)");

    // Start HTTP Thread
    if (ctx->http)
    {
        if (pthread_create(&ctx->http_thread, NULL, http_thread_func, ctx) != 0)
        {
            log_error("[CORE] Failed to start HTTP thread");
        }
        else
        {
            log_info("[CORE] HTTP server thread started");
        }
    }

    while (atomic_load(&ctx->running) && !g_shutdown && !(shutdown_flag && *shutdown_flag))
    {
        // log_info("[CORE] Entering poll...");
        int n = ipc_server_poll(ctx->ipc, events, MAX_EVENTS, 1000);
        // log_info("[CORE] Exited poll, n=%d", n);

        if (n == 0)
        {
            // Timeout - intended behavior for periodic tasks
        }

        if (n < 0)
        {
            if (n == -EINTR)
            {
                // Should not happen often with signalfd, but check flags
                if (shutdown_flag && *shutdown_flag) break;
                if (g_shutdown) break;
                continue;
            }
            log_error("[CORE] epoll_wait failed: %s", strerror(-n));
            break;
        }

        for (int i = 0; i < n; i++)
        {
            void *ptr = events[i].data.ptr;
            uint32_t ev = events[i].events;

            // --- SIGNAL HANDLING ---
            if (ptr == &SIGNAL_FD_TAG)
            {
                struct signalfd_siginfo fdsi;
                ssize_t s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
                if (s == sizeof(struct signalfd_siginfo))
                {
                    if (fdsi.ssi_signo == SIGINT)
                        log_info("[CORE] Received SIGINT via signalfd");
                    else if (fdsi.ssi_signo == SIGTERM)
                        log_info("[CORE] Received SIGTERM via signalfd");

                    atomic_store(&ctx->running, 0);  // Stop loop
                }
                continue;
            }

            // --- IPC HANDLING ---

            // 1. Check for listen socket event (new connection)
            if (ipc_server_is_listen_event(ctx->ipc, ptr))
            {
                ipc_conn *new_conn = NULL;
                if (ipc_server_accept(ctx->ipc, &new_conn) == 0)
                {
                    if (ctx->conn_count < MAX_PLUGIN_CONNECTIONS)
                    {
                        pthread_mutex_lock(&ctx->conn_lock);
                        ctx->conns[ctx->conn_count++] = new_conn;
                        pthread_mutex_unlock(&ctx->conn_lock);
                        log_debug("[IPC] New connection accepted");
                    }
                    else
                    {
                        log_warn("[IPC] Connection rejected: max plugins reached");
                        ipc_server_unregister_conn(ctx->ipc, new_conn);
                        ipc_conn_destroy(&new_conn);
                    }
                }
                continue;
            }

            // 2. Handle client connection event
            ipc_conn *conn = (ipc_conn *) ptr;
            int disconnect = 0;

            if (ev & (EPOLLERR | EPOLLHUP))
            {
                disconnect = 1;
            }

            if (!disconnect && (ev & EPOLLOUT))
            {
                if (ipc_conn_flush(conn) < 0)
                {
                    disconnect = 1;
                }
                else if (!ipc_conn_has_pending(conn))
                {
                    ipc_server_update_conn_events(ctx->ipc, conn, EPOLLIN);
                }
            }

            if (!disconnect && (ev & EPOLLIN))
            {
                char *msg = NULL;
                size_t len = 0;
                int res = ipc_conn_recv(conn, &msg, &len);
                if (res < 0)
                {
                    disconnect = 1;
                }
                else if (res == 0 && msg)
                {
                    char *p = msg;
                    const char *end = NULL;
                    while ((size_t) (p - msg) < len && *p)
                    {
                        while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
                        if (!*p) break;

                        cJSON *json = cJSON_ParseWithOpts(p, &end, 0);
                        if (json)
                        {
                            if (handle_ipc_command(ctx, conn, json) == 0)
                            {
                                cJSON_Delete(json);
                            }
                        }
                        else
                        {
                            break;
                        }
                        p = end;
                    }
                    free(msg);

                    if (ipc_conn_has_pending(conn))
                    {
                        ipc_server_update_conn_events(ctx->ipc, conn, EPOLLIN | EPOLLOUT);
                    }
                }
            }

            if (disconnect)
            {
                // Handle disconnection
                int conn_idx = -1;
                pthread_mutex_lock(&ctx->conn_lock);
                for (int j = 0; j < ctx->conn_count; j++)
                {
                    if (ctx->conns[j] == conn)
                    {
                        conn_idx = j;
                        break;
                    }
                }

                if (conn_idx >= 0)
                {
                    const char *pid = ipc_conn_plugin_id(conn);
                    log_info("[PLUGIN] Disconnected: '%s'", pid ? pid : "unknown");

                    if (pid)
                    {
                        for (int k = 0; k < ctx->registry_count; k++)
                        {
                            if (strcmp(ctx->registry[k].plugin_id, pid) == 0)
                            {
                                ctx->registry[k] = ctx->registry[--ctx->registry_count];
                                k--;
                            }
                        }
                    }

                    ipc_server_unregister_conn(ctx->ipc, conn);
                    ipc_conn_destroy(&conn);

                    ctx->conns[conn_idx] = ctx->conns[ctx->conn_count - 1];
                    ctx->conn_count--;
                }
                pthread_mutex_unlock(&ctx->conn_lock);
            }
        }

        // Periodic tasks
        (void) plugin_mgr_check_health(ctx->plugins);

        static time_t last_tick = 0;
        time_t now = time(NULL);
        if (now - last_tick >= 1)
        {
            db_tick(ctx->db);
            last_tick = now;
        }
    }

    log_info("[CORE] Shutdown sequence initiated");

    // Cleanup signals
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sfd, NULL);
    close(sfd);

    // Stop HTTP Thread
    if (ctx->http)
    {
        log_info("[CORE] Stopping HTTP server...");
        http_server_stop(ctx->http);
        pthread_join(ctx->http_thread, NULL);
        log_info("[CORE] HTTP server stopped");
    }

    // Stop Plugins (send signal or just let them die when IPC closes?)
    // plugin_mgr_stop_all(ctx->plugins); // If we have this function

    atomic_store(&ctx->running, 0);
}

void heimwatt_destroy(heimwatt_ctx **ctx_ptr)
{
    if (!ctx_ptr || !*ctx_ptr) return;
    heimwatt_ctx *ctx = *ctx_ptr;

    log_info("[SHUTDOWN] Cleaning up...");

    // 1. Stop Plugins (Ensure all child processes are killed/waited)
    if (ctx->plugins)
    {
        plugin_mgr_stop_all(ctx->plugins);
    }

    // 2. Stop Thread Pool (Wait for all pending tasks)
    // NOTE: This MUST happen before we destroy resources (IPC/DB) if tasks depend on them.
    if (ctx->pool)
    {
        log_info("[SHUTDOWN] Stopping thread pool...");
        thread_pool_destroy(&ctx->pool);
        log_info("[SHUTDOWN] Thread pool stopped");
    }

    // 3. Destroy HTTP Server (Frees memory, but loop is already stopped)
    if (ctx->http)
    {
        http_server_destroy(&ctx->http);
    }

    // 4. Destroy IPC Server (Closes sockets)
    if (ctx->ipc)
    {
        ipc_server_destroy(&ctx->ipc);
    }

    // 5. Cleanup plugin manager memory
    plugin_mgr_destroy(&ctx->plugins);

    // 6. Close Clean up IPC connections (just in case any remain)
    pthread_mutex_lock(&ctx->conn_lock);
    for (int i = 0; i < ctx->conn_count; i++)
    {
        if (ctx->conns[i]) ipc_conn_destroy(&ctx->conns[i]);
    }
    ctx->conn_count = 0;
    pthread_mutex_unlock(&ctx->conn_lock);

    // 7. Close Database
    if (ctx->db)
    {
        log_info("[SHUTDOWN] Closing database...");
        db_close(&ctx->db);
        log_info("[SHUTDOWN] Database closed");
    }

    log_info("[SHUTDOWN] HeimWatt Core stopped");

    if (ctx->log_file)
    {
        fclose(ctx->log_file);
    }

    pthread_mutex_destroy(&ctx->conn_lock);

    free(ctx);
    *ctx_ptr = NULL;
}

void heimwatt_request_shutdown(heimwatt_ctx *ctx)
{
    if (ctx) atomic_store(&ctx->running, 0);
    g_shutdown = 1;
    log_info("[CORE] Shutdown requested");
}

bool heimwatt_is_running(const heimwatt_ctx *ctx)
{
    return ctx ? atomic_load(&ctx->running) : false;
}
