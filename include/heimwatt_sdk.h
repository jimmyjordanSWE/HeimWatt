#ifndef HEIMWATT_SDK_H
#define HEIMWATT_SDK_H

/**
 * @file heimwatt_sdk.h
 * @brief HeimWatt Plugin SDK v3.0
 *
 * See docs/SDK_SPEC.md for full documentation.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "semantic_types.h"

// ============================================================================
// Core Types
// ============================================================================

typedef struct plugin_ctx plugin_ctx;
typedef struct json_value json_value;
typedef struct sdk_req sdk_req;
typedef struct sdk_resp sdk_resp;

typedef enum
{
    SDK_LOG_DEBUG,
    SDK_LOG_INFO,
    SDK_LOG_WARN,
    SDK_LOG_ERROR
} sdk_log_level;

// ============================================================================
// Lifecycle
// ============================================================================

int sdk_create(plugin_ctx **ctx_out, int argc, char **argv);
int sdk_run(plugin_ctx *ctx);
void sdk_destroy(plugin_ctx **ctx_ptr);

// ============================================================================
// Configuration
// ============================================================================

/**
 * Get config value from manifest.json.
 * @param key Config key name.
 * @param value Output pointer. Caller must free.
 * @return 0 if found, -1 if missing.
 */
int sdk_get_config(plugin_ctx *ctx, const char *key, char **value);

// ============================================================================
// Scheduling
// ============================================================================

typedef void (*sdk_tick_handler)(plugin_ctx *ctx, int64_t timestamp);
typedef void (*sdk_io_handler)(plugin_ctx *ctx, int fd);

/** Register handler for manifest's interval_seconds. */
int sdk_register_ticker(plugin_ctx *ctx, sdk_tick_handler handler);

/** Register cron-style schedule (e.g. "15 * * * *" = every hour at :15). */
int sdk_register_cron(plugin_ctx *ctx, const char *expression, sdk_tick_handler handler);

/** Register file descriptor for event-driven IO. */
int sdk_register_fd(plugin_ctx *ctx, int fd, sdk_io_handler handler);

// ============================================================================
// HTTP
// ============================================================================

/** Simple HTTP GET. Caller frees body_out. Returns HTTP status or -1. */
int sdk_http_get(plugin_ctx *ctx, const char *url, char **body_out);

/** Fetch URL and parse JSON. Caller must call sdk_json_free(). */
int sdk_fetch_json(plugin_ctx *ctx, const char *url, json_value **out);

// ============================================================================
// JSON Utilities
// ============================================================================

json_value *sdk_json_parse(const char *str);
void sdk_json_free(json_value *v);

const json_value *sdk_json_get(const json_value *obj, const char *key);
size_t sdk_json_array_size(const json_value *arr);
const json_value *sdk_json_array_get(const json_value *arr, size_t idx);

const char *sdk_json_string(const json_value *v);
double sdk_json_number(const json_value *v);
int64_t sdk_json_int(const json_value *v);
bool sdk_json_bool(const json_value *v);

// ============================================================================
// Time Utilities
// ============================================================================

/** Parse ISO8601 timestamp to Unix epoch. */
int64_t sdk_time_parse_iso(const char *str);

/** Get current time as Unix epoch. */
int64_t sdk_time_now(void);

/** Iteration macro for time series arrays. */
#define sdk_foreach_ts(arr, entry, ts)                                                     \
    for (size_t _i = 0;                                                                    \
         _i < sdk_json_array_size(arr) &&                                                  \
         ((entry) = sdk_json_array_get(arr, _i),                                           \
         (ts) = sdk_time_parse_iso(sdk_json_string(sdk_json_get(entry, "validTime"))), 1); \
         _i++)

// ============================================================================
// Reporting
// ============================================================================

typedef struct
{
    semantic_type semantic;
    double value;
    int64_t timestamp;     // 0 = Now
    const char *currency;  // ISO 4217 or NULL
} sdk_metric;

/** Full control reporting. */
int sdk_report(plugin_ctx *ctx, const sdk_metric *metric);

/** Shorthand: report by type name (Core looks up ID). */
int sdk_report_value(plugin_ctx *ctx, const char *type_name, double value, int64_t ts);

/** Shorthand for price types with currency. */
int sdk_report_price(plugin_ctx *ctx, const char *type_name, double value, const char *currency,
                     int64_t ts);

/** Report raw/extension data. */
int sdk_report_raw(plugin_ctx *ctx, const char *key, int64_t timestamp, const char *json_fmt, ...);

/** Lookup semantic type ID by name. Cache result for performance. */
semantic_type sdk_type_lookup(plugin_ctx *ctx, const char *name);

// ============================================================================
// State Persistence
// ============================================================================

int sdk_state_save(plugin_ctx *ctx, const char *key, const char *value);
int sdk_state_load(plugin_ctx *ctx, const char *key, char **value_out);

// ============================================================================
// Logging
// ============================================================================

void sdk_log(const plugin_ctx *ctx, sdk_log_level level, const char *fmt, ...);

// ============================================================================
// Calculator Plugins (OUT)
// ============================================================================

typedef int (*sdk_api_handler)(plugin_ctx *ctx, const sdk_req *req, sdk_resp *resp);

// Request Accessors
const char *sdk_req_method(const sdk_req *req);
const char *sdk_req_path(const sdk_req *req);
const char *sdk_req_query_param(const sdk_req *req, const char *key);

int sdk_register_endpoint(plugin_ctx *ctx, const char *method, const char *path,
                          sdk_api_handler handler);

int sdk_require_semantic(plugin_ctx *ctx, semantic_type type);

typedef struct
{
    int64_t timestamp;
    double value;
    char currency[4];
} sdk_data_point;

int sdk_query_latest(plugin_ctx *ctx, semantic_type type, sdk_data_point *out);
int sdk_query_history(plugin_ctx *ctx, semantic_type type, int64_t from_ts, int64_t to_ts,
                      sdk_data_point **out_array, size_t *out_count);
void sdk_data_point_destroy(sdk_data_point **points_ptr);

void sdk_resp_set_status(sdk_resp *resp, int code);
void sdk_resp_set_json(sdk_resp *resp, const char *json_body);
void sdk_resp_set_header(sdk_resp *resp, const char *key, const char *val);

// ============================================================================
// Boilerplate Macro
// ============================================================================

#define HEIMWATT_PLUGIN_ENTRY(init_func)                \
    int main(int argc, char **argv)                     \
    {                                                   \
        plugin_ctx *ctx = NULL;                         \
        if (sdk_create(&ctx, argc, argv) < 0) return 1; \
        if (init_func(ctx) < 0)                         \
        {                                               \
            sdk_destroy(&ctx);                          \
            return 1;                                   \
        }                                               \
        int ret = sdk_run(ctx);                         \
        sdk_destroy(&ctx);                              \
        return ret;                                     \
    }

#endif  // HEIMWATT_SDK_H
