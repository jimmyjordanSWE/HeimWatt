// #define _GNU_SOURCE (Defined in Makefile)
#include "server.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "core/config.h"
#include "core/ipc.h"
#include "core/plugin_mgr.h"
#include "db.h"
#include "libs/cJSON.h"
#include "log.h"
#include "net/http_server.h"

enum
{
    MAX_PLUGIN_CONNECTIONS = 32,
    REQUEST_ID_LEN = 37
};

struct heimwatt_ctx
{
    db_handle *db;
    ipc_server *ipc;
    plugin_mgr *plugins;
    atomic_int running;

    // Connections managed here for Alpha
    ipc_conn *conns[MAX_PLUGIN_CONNECTIONS];
    int conn_count;

    // Logging
    FILE *log_file;

    // HTTP Server
    http_server *http;
    pthread_t http_thread;

    // Endpoint Registry
    struct
    {
        char path[128];
        char plugin_id[64];
        char method[16];
    } registry[32];
    int registry_count;

    // Connection Lock (Protect conns array and registry)
    pthread_mutex_t conn_lock;
    // Global Config
    double lat;
    double lon;
    char area[16];
};

// Global for signal handling if needed, but we pass ctx via args
static volatile int g_shutdown = 0;

heimwatt_ctx *heimwatt_create(void)
{
    heimwatt_ctx *ctx = malloc(sizeof(*ctx));
    if (ctx)
    {
        memset(ctx, 0, sizeof(*ctx));
        pthread_mutex_init(&ctx->conn_lock, NULL);
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
    free(str);
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
    free(str);
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
        free(str);

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
    heimwatt_ctx *ctx = (heimwatt_ctx *) arg;
    http_server_run(ctx->http);
    return NULL;
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
    char db_open_path[512];
    snprintf(config_path, sizeof(config_path), "config/heimwatt.json");
    if (config_load(cfg, config_path) == 0)
    {
        const char *loc_name = config_get_loc_name(cfg);
        log_info("[INIT] Config loaded. CSV Interval: %ds, Location: %s",
                 config_get_csv_interval(cfg), loc_name);

        ctx->lat = config_get_lat(cfg);
        ctx->lon = config_get_lon(cfg);
        snprintf(ctx->area, sizeof(ctx->area), "%s", config_get_area(cfg));

        // Use location name in DB path if not default
        if (strcmp(loc_name, "default") != 0)
        {
            // Append coordinates to disambiguate locations (e.g., stockholm_59.33N_18.07E)
            char lat_dir = (ctx->lat >= 0) ? 'N' : 'S';
            char lon_dir = (ctx->lon >= 0) ? 'E' : 'W';
            snprintf(db_open_path, sizeof(db_open_path), "%s/%s_%.2f%c_%.2f%c", path, loc_name,
                     fabs(ctx->lat), lat_dir, fabs(ctx->lon), lon_dir);
        }
        else
        {
            snprintf(db_open_path, sizeof(db_open_path), "%s/default", path);
        }
    }
    else
    {
        snprintf(db_open_path, sizeof(db_open_path), "%s/default", path);
    }

    // 1. Open DB
    ret = db_open(&ctx->db, db_open_path);
    if (ret < 0)
    {
        log_error("[INIT] Failed to open DB at %s: %s", db_open_path, strerror(-ret));
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

static void handle_json(heimwatt_ctx *ctx, ipc_conn *conn, cJSON *json)
{
    if (!json) return;

    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
    if (!cmd || !cmd->valuestring)
    {
        return;
    }

    const char *plugin_id = ipc_conn_plugin_id(conn);
    const char *from = plugin_id ? plugin_id : "unknown";

    log_debug("[IPC] Received cmd='%s' from='%s'", cmd->valuestring, from);

    if (strcmp(cmd->valuestring, "report") == 0)
    {
        // {"cmd":"report", "type":"...", "val":123, "ts": ...}
        cJSON *type = cJSON_GetObjectItem(json, "type");
        cJSON *val = cJSON_GetObjectItem(json, "val");
        cJSON *ts = cJSON_GetObjectItem(json, "ts");

        if (type && val && ts)
        {
            semantic_type st = semantic_from_string(type->valuestring);
            int64_t timestamp = (int64_t) ts->valuedouble;

            int ret = db_insert_tier1(ctx->db, st, timestamp, val->valuedouble, NULL, from);
            if (ret == -EEXIST)
            {
                log_debug("[DATA] Skip (Cached): %s @ %ld", type->valuestring, timestamp);
                // Still update last run? Yes, it means plugin tried.
                plugin_mgr_set_last_run(ctx->plugins, from, (time_t) time(NULL));
                return;
            }

            if (ret < 0)
            {
                log_error("[DATA] Failed to store %s: %s", type->valuestring,
                          db_error_message(ctx->db) ? db_error_message(ctx->db) : strerror(-ret));
                return;
            }

            // Success
            plugin_mgr_set_last_run(ctx->plugins, from, (time_t) time(NULL));

            time_t t = (time_t) timestamp;
            struct tm tm_info;
            (void) localtime_r(&t, &tm_info);
            char time_buf[64];
            (void) strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

            log_info("[DATA] Stored: %s = %.2f @ %s (from: %s)", type->valuestring,
                     val->valuedouble, time_buf, from);
        }
    }
    else if (strcmp(cmd->valuestring, "config") == 0)
    {
        // {"cmd":"config", "key":"..."}
        cJSON *key = cJSON_GetObjectItem(json, "key");
        if (key && key->valuestring)
        {
            char resp[2048];
            const char *k = key->valuestring;

            const char *src = plugin_mgr_get_config(ctx->plugins, from, k);
            if (!src) src = "";  // Or send error/empty

            // Perform Variable Substitution
            char final_val[1024];
            char *dst = final_val;
            char *end = final_val + sizeof(final_val) - 1;

            while (*src && dst < end)
            {
                if (strncmp(src, "{lat}", 5) == 0)
                {
                    dst += snprintf(dst, end - dst, "%.4f", ctx->lat);
                    src += 5;
                }
                else if (strncmp(src, "{lon}", 5) == 0)
                {
                    dst += snprintf(dst, end - dst, "%.4f", ctx->lon);
                    src += 5;
                }
                else if (strncmp(src, "{area}", 6) == 0)
                {
                    dst += snprintf(dst, end - dst, "%s", ctx->area);
                    src += 6;
                }
                else
                {
                    *dst++ = *src++;
                }
            }
            *dst = '\0';

            log_debug("[IPC] Config request: key='%s' -> val='%s'", k, final_val);

            (void) snprintf(resp, sizeof(resp), "{\"val\":\"%s\"}\n", final_val);
            ipc_conn_send(conn, resp, strlen(resp));
        }
    }
    else if (strcmp(cmd->valuestring, "lookup") == 0)
    {
        cJSON *name = cJSON_GetObjectItem(json, "name");
        if (name && name->valuestring)
        {
            semantic_type id = semantic_from_string(name->valuestring);
            char resp[256];
            (void) snprintf(resp, sizeof(resp), "{\"id\":%d}\n", (int) id);
            ipc_conn_send(conn, resp, strlen(resp));
            log_debug("[IPC] Type lookup: '%s' -> %d", name->valuestring, (int) id);
        }
    }
    else if (strcmp(cmd->valuestring, "check_data") == 0)
    {
        // {"cmd":"check_data", "ts":..., "sem":...}
        cJSON *ts_json = cJSON_GetObjectItem(json, "ts");
        cJSON *sem_json = cJSON_GetObjectItem(json, "sem");

        int exists = 0;
        if (ts_json)
        {
            int64_t ts = (int64_t) ts_json->valuedouble;
            semantic_type sem =
                sem_json ? (semantic_type) sem_json->valueint : SEM_ATMOSPHERE_TEMPERATURE;
            exists = db_query_point_exists_tier1(ctx->db, sem, ts) > 0;
        }

        char resp[64];
        (void) snprintf(resp, sizeof(resp), "{\"exists\":%s}\n", exists ? "true" : "false");
        ipc_conn_send(conn, resp, strlen(resp));
        log_debug("[IPC] check_data: exists=%d", exists);
    }
    else if (strcmp(cmd->valuestring, "query_range") == 0)
    {
        cJSON *sem = cJSON_GetObjectItem(json, "sem");
        cJSON *from_json = cJSON_GetObjectItem(json, "from");
        cJSON *to_json = cJSON_GetObjectItem(json, "to");

        if (sem && from_json && to_json)
        {
            semantic_type type = (semantic_type) sem->valueint;
            int64_t from_ts = (int64_t) from_json->valuedouble;
            int64_t to_ts = (int64_t) to_json->valuedouble;

            double *values = NULL;
            int64_t *ts_arr = NULL;
            size_t count = 0;

            int ret = db_query_range_tier1(ctx->db, type, from_ts, to_ts, &values, &ts_arr, &count);
            if (ret == 0)
            {
                cJSON *resp_json = cJSON_CreateObject();
                cJSON_AddNumberToObject(resp_json, "count", (double) count);

                cJSON *vals_arr = cJSON_AddArrayToObject(resp_json, "values");
                cJSON *tss_arr = cJSON_AddArrayToObject(resp_json, "ts");

                for (size_t i = 0; i < count; i++)
                {
                    cJSON_AddItemToArray(vals_arr, cJSON_CreateNumber(values[i]));
                    cJSON_AddItemToArray(tss_arr, cJSON_CreateNumber((double) ts_arr[i]));
                }

                char *resp_str = cJSON_PrintUnformatted(resp_json);
                if (resp_str)
                {
                    ipc_conn_send(conn, resp_str, strlen(resp_str));
                    ipc_conn_send(conn, "\n", 1);
                    free(resp_str);
                }
                cJSON_Delete(resp_json);

                db_free(values);
                db_free(ts_arr);
                log_debug("[IPC] Query range: sem=%d from=%ld to=%ld -> count=%zu", (int) type,
                          from_ts, to_ts, count);
            }
            else
            {
                char err_resp[] = "{\"count\":0, \"error\":\"DB Error\"}\n";
                ipc_conn_send(conn, err_resp, strlen(err_resp));
                log_error("[IPC] Query range failed: %d", ret);
            }
        }
    }
    else if (strcmp(cmd->valuestring, "query_latest") == 0)
    {
        // {"cmd":"query_latest", "sem":...}
        cJSON *sem = cJSON_GetObjectItem(json, "sem");
        if (sem)
        {
            semantic_type type = (semantic_type) sem->valueint;
            double val = 0;
            int64_t ts = 0;

            int ret = db_query_latest_tier1(ctx->db, type, &val, &ts);
            if (ret == 0)
            {
                // Found
                char resp[128];
                snprintf(resp, sizeof(resp), "{\"val\":%.6f, \"ts\":%ld}\n", val, ts);
                ipc_conn_send(conn, resp, strlen(resp));
                log_debug("[IPC] Query latest: sem=%d -> %.2f @ %ld", (int) type, val, ts);
            }
            else
            {
                // Not found or error (return empty object or error?)
                const char *empty = "{}\n";
                ipc_conn_send(conn, empty, strlen(empty));
                log_debug("[IPC] Query latest: sem=%d -> Not found", (int) type);
            }
        }
    }
    else if (strcmp(cmd->valuestring, "register_endpoint") == 0)
    {
        cJSON *method = cJSON_GetObjectItem(json, "method");
        cJSON *path = cJSON_GetObjectItem(json, "path");

        pthread_mutex_lock(&ctx->conn_lock);
        if (method && path && plugin_id && ctx->registry_count < 32)
        {
            int idx = ctx->registry_count++;
            snprintf(ctx->registry[idx].path, sizeof(ctx->registry[0].path), "%s",
                     path->valuestring);
            snprintf(ctx->registry[idx].method, sizeof(ctx->registry[0].method), "%s",
                     method->valuestring);
            snprintf(ctx->registry[idx].plugin_id, sizeof(ctx->registry[0].plugin_id), "%s",
                     plugin_id);
            log_info("[IPC] Registered endpoint %s %s -> %s", method->valuestring,
                     path->valuestring, plugin_id);
        }
        pthread_mutex_unlock(&ctx->conn_lock);
    }
    else if (strcmp(cmd->valuestring, "http_response") == 0)
    {
        // Async response from plugin - complete the HTTP request
        cJSON *request_id_json = cJSON_GetObjectItem(json, "request_id");
        cJSON *status = cJSON_GetObjectItem(json, "status");
        cJSON *body = cJSON_GetObjectItem(json, "body");

        if (request_id_json && request_id_json->valuestring)
        {
            http_response resp;
            http_response_init(&resp);

            if (status) resp.status_code = status->valueint;
            if (body && body->valuestring)
            {
                http_response_set_json(&resp, body->valuestring);
            }

            int ret = http_server_complete_request(ctx->http, request_id_json->valuestring, &resp);
            if (ret < 0)
            {
                log_warn("[IPC] http_response: request_id not found: %.8s...",
                         request_id_json->valuestring);
            }
            else
            {
                log_debug("[IPC] http_response completed: %.8s...", request_id_json->valuestring);
            }

            http_response_destroy(&resp);
        }
    }
    else if (strcmp(cmd->valuestring, "request_data") == 0)
    {
        // Calculator requesting on-demand data fetch
        cJSON *types = cJSON_GetObjectItem(json, "semantic_types");
        if (types && cJSON_IsArray(types))
        {
            log_info("[IPC] Data request from %s for %d types", from, cJSON_GetArraySize(types));

            cJSON *type_item = NULL;
            cJSON_ArrayForEach(type_item, types)
            {
                if (cJSON_IsString(type_item))
                {
                    const char *semantic_type = type_item->valuestring;
                    const char *providers[32];
                    int provider_count = 0;
                    find_providers_for_type(ctx->plugins, semantic_type, providers, 32,
                                            &provider_count);

                    if (provider_count == 0)
                    {
                        log_warn("[IPC] No provider for %s (requested by %s)", semantic_type, from);
                    }
                    else
                    {
                        log_info("[IPC] Would trigger %s to fetch %s", providers[0], semantic_type);
                        // Note: Full IPC routing to send fetch_now to specific plugin
                        // requires connection map (plugin_id -> ipc_conn)
                        // This infrastructure works; production would need connection tracking
                    }
                }
            }

            char resp[] = "{\"status\":\"acknowledged\"}\n";
            ipc_conn_send(conn, resp, strlen(resp));
        }
    }
    else if (strcmp(cmd->valuestring, "log") == 0)
    {
        // Echo log from plugin
        cJSON *msg = cJSON_GetObjectItem(json, "msg");
        cJSON *level = cJSON_GetObjectItem(json, "level");
        if (msg && msg->valuestring)
        {
            int lvl = level ? level->valueint : LOG_INFO;
            // Map SDK levels to log.c levels
            switch (lvl)
            {
                case 0:
                    log_debug("[SDK:%s] %s", from, msg->valuestring);
                    break;
                case 1:
                    log_info("[SDK:%s] %s", from, msg->valuestring);
                    break;
                case 2:
                    log_warn("[SDK:%s] %s", from, msg->valuestring);
                    break;
                case 3:
                    log_error("[SDK:%s] %s", from, msg->valuestring);
                    break;
                default:
                    log_info("[SDK:%s] %s", from, msg->valuestring);
                    break;
            }
        }
    }
    else if (strcmp(cmd->valuestring, "hello") == 0)
    {
        // {"cmd":"hello", "id":"..."}
        cJSON *id = cJSON_GetObjectItem(json, "id");
        if (id && id->valuestring)
        {
            ipc_conn_set_plugin_id(conn, id->valuestring);
            log_info("[PLUGIN] Connected: '%s'", id->valuestring);
        }
    }
    else
    {
        log_warn("[IPC] Unknown command: '%s' from '%s'", cmd->valuestring, from);
    }
}

void heimwatt_run(heimwatt_ctx *ctx) { heimwatt_run_with_shutdown_flag(ctx, NULL); }

void heimwatt_run_with_shutdown_flag(heimwatt_ctx *ctx, const volatile sig_atomic_t *shutdown_flag)
{
    if (!ctx) return;
    atomic_store(&ctx->running, 1);

    struct pollfd fds[33];  // 1 server + 32 clients

    log_info("[CORE] Main loop started (max %d plugins)", MAX_PLUGIN_CONNECTIONS);

    // Start HTTP Thread
    if (ctx->http)
    {
        if (pthread_create(&ctx->http_thread, NULL, http_thread_func, ctx) != 0)
        {
            log_error("[CORE] Failed to start HTTP thread");
        }
        else
        {
            log_info("[CORE] HTTP server thread started (epoll-based, non-blocking)");
        }
    }

    while (atomic_load(&ctx->running) && !g_shutdown && !(shutdown_flag && *shutdown_flag))
    {
        // Setup poll
        int nfds = 0;
        fds[nfds].fd = ipc_server_fd(ctx->ipc);
        fds[nfds].events = POLLIN;
        nfds++;

        for (int i = 0; i < ctx->conn_count; i++)
        {
            fds[nfds].fd = ipc_conn_fd(ctx->conns[i]);
            fds[nfds].events = POLLIN;
            if (ipc_conn_has_pending(ctx->conns[i]))
            {
                fds[nfds].events |= POLLOUT;
            }
            nfds++;
        }

        if (poll(fds, nfds, 1000) < 0)
        {
            if (errno == EINTR) continue;
            break;
        }

        // CSV Backend Tick (Flush check)
        db_tick(ctx->db);

        // Check Server
        if (fds[0].revents & POLLIN)
        {
            ipc_conn *new_conn = NULL;
            if (ipc_server_accept(ctx->ipc, &new_conn) == 0)
            {
                if (ctx->conn_count < MAX_PLUGIN_CONNECTIONS)
                {
                    pthread_mutex_lock(&ctx->conn_lock);
                    ctx->conns[ctx->conn_count++] = new_conn;
                    pthread_mutex_unlock(&ctx->conn_lock);
                    log_debug("[IPC] New connection accepted (pending hello)");
                }
                else
                {
                    log_warn("[IPC] Connection rejected: max plugins reached (%d)",
                             MAX_PLUGIN_CONNECTIONS);
                    ipc_conn_destroy(&new_conn);
                }
            }
        }

        // Check Clients
        for (int i = 0; i < ctx->conn_count; i++)
        {
            int idx = i + 1;
            int disconnect = 0;

            // Handle OUTPUT
            if (fds[idx].revents & POLLOUT)
            {
                if (ipc_conn_flush(ctx->conns[i]) < 0)
                {
                    disconnect = 1;
                }
            }

            // Handle INPUT (if still connected)
            if (!disconnect && (fds[idx].revents & POLLIN))
            {
                char *msg = NULL;
                size_t len = 0;
                int ret = ipc_conn_recv(ctx->conns[i], &msg, &len);
                if (ret < 0)
                {
                    disconnect = 1;
                }
                else
                {
                    // Handle potential multiple JSON objects in one buffer
                    const char *p = msg;
                    const char *end = NULL;
                    while ((size_t) (p - msg) < len && *p)
                    {
                        // Skip whitespace
                        while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
                        if (!*p) break;

                        cJSON *json = cJSON_ParseWithOpts(p, &end, 0);
                        if (json)
                        {
                            handle_json(ctx, ctx->conns[i], json);
                            cJSON_Delete(json);
                        }
                        else
                        {
                            break;  // Error or incomplete
                        }
                        p = end;
                    }
                    free(msg);
                }
            }

            if (disconnect)
            {
                // Disconnect
                const char *pid = ipc_conn_plugin_id(ctx->conns[i]);
                log_info("[PLUGIN] Disconnected: '%s'", pid ? pid : "unknown");

                pthread_mutex_lock(&ctx->conn_lock);
                ipc_conn_destroy(&ctx->conns[i]);
                // Shift
                ctx->conns[i] = ctx->conns[ctx->conn_count - 1];
                ctx->conn_count--;
                pthread_mutex_unlock(&ctx->conn_lock);

                // Do not i--. The moved conn will be checked next iteration.
            }
        }

        // Check plugin health, restart any that crashed
        (void) plugin_mgr_check_health(ctx->plugins);
    }

    // Stop HTTP Thread
    if (ctx->http)
    {
        log_info("[CORE] Stopping HTTP server...");
        http_server_stop(ctx->http);
        pthread_join(ctx->http_thread, NULL);
        log_info("[CORE] HTTP server stopped");
    }

    // Log shutdown reason (safe here, outside signal context)
    if (shutdown_flag && *shutdown_flag)
    {
        log_info("[CORE] Shutdown requested via signal");
    }

    log_info("[CORE] Main loop exiting...");
}

void heimwatt_destroy(heimwatt_ctx **ctx_ptr)
{
    if (!ctx_ptr || !*ctx_ptr) return;
    heimwatt_ctx *ctx = *ctx_ptr;

    log_info("[SHUTDOWN] Cleaning up...");

    // Stop plugins first
    plugin_mgr_stop_all(ctx->plugins);
    plugin_mgr_destroy(&ctx->plugins);

    db_close(&ctx->db);
    ipc_server_destroy(&ctx->ipc);
    http_server_destroy(&ctx->http);

    for (int i = 0; i < ctx->conn_count; i++)
    {
        ipc_conn_destroy(&ctx->conns[i]);
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
