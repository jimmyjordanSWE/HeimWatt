
#include "db.h"
#include "ipc_handlers.h"
#include "log.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "core/config.h"
#include "core/plugin_mgr.h"
#include "net/http_server.h"
#include "util/mem.h"
#include "util/thread_pool.h"

/* ============================================================================
 * Helper Prototypes
 * ============================================================================ */

#include "server_internal.h"

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

/* ============================================================================ */

static void cmd_report_logic(heimwatt_ctx* ctx, cJSON* json, const char* plugin_id) {
    const char* from = plugin_id ? plugin_id : "unknown";

    // {"cmd":"report", "type":"...", "val":123, "ts": ...}
    cJSON* type = cJSON_GetObjectItem(json, "type");
    cJSON* val = cJSON_GetObjectItem(json, "val");
    cJSON* ts = cJSON_GetObjectItem(json, "ts");

    if (type && val && ts) {
        semantic_type st = semantic_from_string(type->valuestring);
        int64_t timestamp = (int64_t) ts->valuedouble;

        int ret = db_insert_tier1(ctx->db, st, timestamp, val->valuedouble, NULL, from);
        if (ret == -EEXIST) {
            log_debug("[DATA] Skip (Cached): %s @ %ld", type->valuestring, timestamp);
            plugin_mgr_set_last_run(ctx->plugins, from, time(NULL));
            return;
        }

        if (ret < 0) {
            log_error("[DATA] Failed to store %s: %s", type->valuestring, strerror(-ret));
            return;
        }

        // Success
        plugin_mgr_set_last_run(ctx->plugins, from, time(NULL));

        time_t t = (time_t) timestamp;
        struct tm tm_info;
        (void) localtime_r(&t, &tm_info);
        char time_buf[64];
        (void) strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

        log_info("[DATA] Stored: %s = %.2f @ %s (from: %s)", type->valuestring, val->valuedouble,
                 time_buf, from);
    }
}

typedef struct {
    heimwatt_ctx* ctx;
    cJSON* json;
    char plugin_id[64];
} report_task_args;

static void cmd_report_task(void* arg) {
    report_task_args* args = (report_task_args*) arg;
    cmd_report_logic(args->ctx, args->json, args->plugin_id);
    cJSON_Delete(args->json);  // Ownership was transferred
    mem_free(args);
}

static void cmd_report_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    (void) conn;
    const char* from = ipc_conn_plugin_id(conn);
    cmd_report_logic(ctx, json, from);
}

static void cmd_config_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    const char* from = ipc_conn_plugin_id(conn);
    if (!from)
        from = "unknown";

    // {"cmd":"config", "key":"..."}
    cJSON* key = cJSON_GetObjectItem(json, "key");
    if (key && key->valuestring) {
        char resp[2048];
        const char* k = key->valuestring;

        const char* src = plugin_mgr_get_config(ctx->plugins, from, k);
        if (!src)
            src = "";

        // Perform Variable Substitution
        char final_val[1024];
        char* dst = final_val;
        char* end = final_val + sizeof(final_val) - 1;

        while (*src && dst < end) {
            int n = 0;
            if (strncmp(src, "{lat}", 5) == 0) {
                n = snprintf(dst, end - dst, "%.4f", ctx->lat);
                src += 5;
            } else if (strncmp(src, "{lon}", 5) == 0) {
                n = snprintf(dst, end - dst, "%.4f", ctx->lon);
                src += 5;
            } else if (strncmp(src, "{area}", 6) == 0) {
                n = snprintf(dst, end - dst, "%s", ctx->area);
                src += 6;
            } else {
                *dst++ = *src++;
                continue;
            }

            if (n < 0)
                break;  // Error
            if (n >= (end - dst)) {
                dst = end;  // Truncated
                break;
            }
            dst += n;
        }
        *dst = '\0';

        log_debug("[IPC] Config request: key='%s' -> val='%s'", k, final_val);

        (void) snprintf(resp, sizeof(resp), "{\"val\":\"%s\"}\n", final_val);
        ipc_conn_send(conn, resp, strlen(resp));
    }
}

static void cmd_lookup_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    (void) ctx;
    cJSON* name = cJSON_GetObjectItem(json, "name");
    if (name && name->valuestring) {
        semantic_type id = semantic_from_string(name->valuestring);
        char resp[256];
        (void) snprintf(resp, sizeof(resp), "{\"id\":%d}\n", (int) id);
        ipc_conn_send(conn, resp, strlen(resp));
        log_debug("[IPC] Type lookup: '%s' -> %d", name->valuestring, (int) id);
    }
}

static void cmd_check_data_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    // {"cmd":"check_data", "ts":..., "sem":...}
    cJSON* ts_json = cJSON_GetObjectItem(json, "ts");
    cJSON* sem_json = cJSON_GetObjectItem(json, "sem");

    int exists = 0;
    if (ts_json) {
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

static void cmd_query_range_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    cJSON* sem = cJSON_GetObjectItem(json, "sem");
    cJSON* from_json = cJSON_GetObjectItem(json, "from");
    cJSON* to_json = cJSON_GetObjectItem(json, "to");

    if (sem && from_json && to_json) {
        semantic_type type = (semantic_type) sem->valueint;
        int64_t from_ts = (int64_t) from_json->valuedouble;
        int64_t to_ts = (int64_t) to_json->valuedouble;

        double* values = NULL;
        int64_t* ts_arr = NULL;
        size_t count = 0;

        int ret = db_query_range_tier1(ctx->db, type, from_ts, to_ts, &values, &ts_arr, &count);
        if (ret == 0) {
            cJSON* resp_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp_json, "count", (double) count);

            cJSON* vals_arr = cJSON_AddArrayToObject(resp_json, "values");
            cJSON* tss_arr = cJSON_AddArrayToObject(resp_json, "ts");

            for (size_t i = 0; i < count; i++) {
                cJSON_AddItemToArray(vals_arr, cJSON_CreateNumber(values[i]));
                cJSON_AddItemToArray(tss_arr, cJSON_CreateNumber((double) ts_arr[i]));
            }

            char* resp_str = cJSON_PrintUnformatted(resp_json);
            if (resp_str) {
                ipc_conn_send(conn, resp_str, strlen(resp_str));
                ipc_conn_send(conn, "\n", 1);
                mem_free(resp_str);
            }
            cJSON_Delete(resp_json);

            db_free(values);
            db_free(ts_arr);
            log_debug("[IPC] Query range: sem=%d from=%ld to=%ld -> count=%zu", (int) type, from_ts,
                      to_ts, count);
        } else {
            char err_resp[] = "{\"count\":0, \"error\":\"DB Error\"}\n";
            ipc_conn_send(conn, err_resp, strlen(err_resp));
            log_error("[IPC] Query range failed: %d", ret);
        }
    }
}

static void cmd_query_latest_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    // {"cmd":"query_latest", "sem":...}
    cJSON* sem = cJSON_GetObjectItem(json, "sem");
    if (sem) {
        semantic_type type = (semantic_type) sem->valueint;
        double val = 0;
        int64_t ts = 0;

        int ret = db_query_latest_tier1(ctx->db, type, &val, &ts);
        if (ret == 0) {
            // Found
            char resp[128];
            snprintf(resp, sizeof(resp), "{\"val\":%.6f, \"ts\":%ld}\n", val, ts);
            ipc_conn_send(conn, resp, strlen(resp));
            log_debug("[IPC] Query latest: sem=%d -> %.2f @ %ld", (int) type, val, ts);
        } else {
            // Not found or error
            const char* empty = "{}\n";
            ipc_conn_send(conn, empty, strlen(empty));
            log_debug("[IPC] Query latest: sem=%d -> Not found", (int) type);
        }
    }
}

static void cmd_register_endpoint_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    const char* from = ipc_conn_plugin_id(conn);
    if (!from)
        return;

    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* path = cJSON_GetObjectItem(json, "path");

    pthread_mutex_lock(&ctx->conn_lock);
    if (method && path && ctx->registry_count < 32) {
        int idx = ctx->registry_count++;
        snprintf(ctx->registry[idx].path, sizeof(ctx->registry[0].path), "%s", path->valuestring);
        snprintf(ctx->registry[idx].method, sizeof(ctx->registry[0].method), "%s",
                 method->valuestring);
        snprintf(ctx->registry[idx].plugin_id, sizeof(ctx->registry[0].plugin_id), "%s", from);
        log_info("[IPC] Registered endpoint %s %s -> %s", method->valuestring, path->valuestring,
                 from);
    }
    pthread_mutex_unlock(&ctx->conn_lock);
}

static void cmd_http_response_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    (void) conn;
    // Async response from plugin - complete the HTTP request
    cJSON* request_id_json = cJSON_GetObjectItem(json, "request_id");
    cJSON* status = cJSON_GetObjectItem(json, "status");
    cJSON* body = cJSON_GetObjectItem(json, "body");

    if (request_id_json && request_id_json->valuestring) {
        http_response resp;
        http_response_init(&resp);

        if (status)
            resp.status_code = status->valueint;
        if (body && body->valuestring) {
            http_response_set_json(&resp, body->valuestring);
        }

        int ret = http_server_complete_request(ctx->http, request_id_json->valuestring, &resp);
        if (ret < 0) {
            log_warn("[IPC] http_response: request_id not found: %.8s...",
                     request_id_json->valuestring);
        } else {
            log_debug("[IPC] http_response completed: %.8s...", request_id_json->valuestring);
        }

        http_response_destroy(&resp);
    }
}

static void cmd_request_data_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    const char* from = ipc_conn_plugin_id(conn);
    if (!from)
        from = "unknown";

    // Calculator requesting on-demand data fetch
    cJSON* types = cJSON_GetObjectItem(json, "semantic_types");
    if (types && cJSON_IsArray(types)) {
        log_info("[IPC] Data request from %s for %d types", from, cJSON_GetArraySize(types));

        cJSON* type_item = NULL;
        cJSON_ArrayForEach(type_item, types) {
            if (cJSON_IsString(type_item)) {
                const char* semantic_type = type_item->valuestring;
                const char* providers[32];
                int provider_count = 0;
                find_providers_for_type(ctx->plugins, semantic_type, providers, 32,
                                        &provider_count);

                if (provider_count == 0) {
                    log_warn("[IPC] No provider for %s (requested by %s)", semantic_type, from);
                } else {
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

static void cmd_log_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    (void) ctx;
    const char* from = ipc_conn_plugin_id(conn);
    if (!from)
        from = "unknown";

    // Echo log from plugin
    cJSON* msg = cJSON_GetObjectItem(json, "msg");
    cJSON* level = cJSON_GetObjectItem(json, "level");
    if (msg && msg->valuestring) {
        int lvl = level ? level->valueint : LOG_INFO;
        // Map SDK levels to log.c levels
        switch (lvl) {
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

static void cmd_hello_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    (void) ctx;
    // {"cmd":"hello", "id":"..."}
    cJSON* id = cJSON_GetObjectItem(json, "id");
    if (id && id->valuestring) {
        ipc_conn_set_plugin_id(conn, id->valuestring);
        log_info("[PLUGIN] Connected: '%s'", id->valuestring);
    }
}

static void cmd_credential_get_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    (void) ctx;
    const char* from = ipc_conn_plugin_id(conn);
    if (!from)
        from = "unknown";

    // {"cmd":"credential_get", "key":"..."}
    cJSON* key = cJSON_GetObjectItem(json, "key");
    if (key && key->valuestring) {
        // TODO: Actual credential store lookup
        // For now, return a mock token
        log_info("[IPC] Credential request from %s for key='%s'", from, key->valuestring);

        char resp[] = "{\"value\":\"mock_token_123\"}\n";
        ipc_conn_send(conn, resp, strlen(resp));
    } else {
        char resp[] = "{\"error\":\"Missing key\"}\n";
        ipc_conn_send(conn, resp, strlen(resp));
    }
}

static void cmd_device_setpoint_handler(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    const char* from = ipc_conn_plugin_id(conn);
    if (!from)
        return;

    // Check capability
    plugin_handle* h = plugin_mgr_get(ctx->plugins, from);
    if (!h) {
        log_warn("[IPC] Unknown plugin %s tried to actuate", from);
        return;
    }

    if (!plugin_handle_has_capability(h, CAP_ACTUATE)) {
        log_warn("[IPC] Permission Denied: Plugin %s missing 'actuate' cap", from);
        char resp[] = "{\"error\":\"Permission Denied: Missing 'actuate' capability\"}\n";
        ipc_conn_send(conn, resp, strlen(resp));
        return;
    }

    // {"cmd":"device_setpoint", "id":"...", "value":...}
    cJSON* id = cJSON_GetObjectItem(json, "id");
    cJSON* val = cJSON_GetObjectItem(json, "value");

    if (id && id->valuestring && val) {
        log_info("[IPC] ACTUATE: %s -> %s = %.2f", from, id->valuestring, val->valuedouble);
        // TODO: Route to owner of device
        char resp[] = "{\"status\":\"ok\"}\n";
        ipc_conn_send(conn, resp, strlen(resp));
    }
}

/* ============================================================================
 * Dispatch Table
 * ============================================================================ */

static const struct {
    const char* name;
    void (*fn)(heimwatt_ctx*, ipc_conn*, cJSON*);
} ipc_handlers[] = {
    {"report", cmd_report_handler},
    {"config", cmd_config_handler},
    {"lookup", cmd_lookup_handler},
    {"check_data", cmd_check_data_handler},
    {"query_range", cmd_query_range_handler},
    {"query_latest", cmd_query_latest_handler},
    {"register_endpoint", cmd_register_endpoint_handler},
    {"http_response", cmd_http_response_handler},
    {"request_data", cmd_request_data_handler},
    {"log", cmd_log_handler},
    {"hello", cmd_hello_handler},
    {"credential_get", cmd_credential_get_handler},
    {"device_setpoint", cmd_device_setpoint_handler},
};

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

int handle_ipc_command(heimwatt_ctx* ctx, ipc_conn* conn, cJSON* json) {
    if (!ctx || !conn || !json)
        return 0;

    cJSON* cmd = cJSON_GetObjectItem(json, "cmd");
    if (!cmd || !cmd->valuestring)
        return 0;

    const char* from = ipc_conn_plugin_id(conn);
    if (!from)
        from = "unknown";

    log_debug("[IPC] Received cmd='%s' from='%s'", cmd->valuestring, from);

    // Optimization: Offload 'report' to thread pool
    if (ctx->pool && strcmp(cmd->valuestring, "report") == 0) {
        report_task_args* args = mem_alloc(sizeof(report_task_args));
        if (args) {
            args->ctx = ctx;
            // Assuming json is OWNED by the caller (server.c) and we take it.
            // But cJSON struct structure is tricky to deep copy efficiently if we don't own it.
            // server.c loop: calls this, then cJSON_Delete.
            // Ideally we need to tell server.c "I took it".
            // So we use standard pointer provided.
            args->json = json;

            if (from)
                snprintf(args->plugin_id, sizeof(args->plugin_id), "%s", from);
            else
                args->plugin_id[0] = '\0';

            if (thread_pool_submit(ctx->pool, cmd_report_task, args) == 0) {
                return 1;  // Adopted, caller should not delete json
            }
            mem_free(args);  // Submission failed, free args
        }
        // Fallback to sync handling if thread pool is not available or submission failed
    }

    // Handle all commands synchronously (including 'report' if not offloaded)
    for (size_t i = 0; i < ARRAY_LEN(ipc_handlers); i++) {
        if (strcmp(cmd->valuestring, ipc_handlers[i].name) == 0) {
            ipc_handlers[i].fn(ctx, conn, json);
            return 0;  // Handled synchronously, caller should delete json
        }
    }

    log_warn("[IPC] Unknown command from %s: %s", from, cmd->valuestring);
    return 0;  // Not adopted, caller should delete json
}
