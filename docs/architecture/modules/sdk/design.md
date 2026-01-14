# SDK Design

> **Version**: 3.0 (2026-01-14)  
> **Status**: ✅ API Defined  
> **Scope**: Plugin SDK public API and internal implementation

---

## Overview

The **SDK** (`libheimwatt-sdk.so`) is the library that plugin developers link against. It provides:

1. **Context management**: `sdk_init()`, `sdk_fini()`, `sdk_run()`
2. **Data reporting**: Builder pattern for submitting metrics (IN plugins)
3. **Data querying**: Request data from Core (OUT plugins)
4. **Endpoint registration**: Declare HTTP routes (OUT plugins)
5. **IPC**: Communication with Core (transparent to plugin author)

---

## Directory Structure

```
include/
└── heimwatt_sdk.h      # Public API (shipped to plugin devs)

src/sdk/
├── sdk_core.h / sdk_core.c       # Context lifecycle
├── sdk_report.h / sdk_report.c   # Data reporting
├── sdk_query.h / sdk_query.c     # Data querying
├── sdk_endpoint.h / sdk_endpoint.c # Endpoint registration
└── sdk_ipc.h / sdk_ipc.c         # IPC client
```

---

## Public API: heimwatt_sdk.h

This is the only header plugin developers need.

```c
#ifndef HEIMWATT_SDK_H
#define HEIMWATT_SDK_H

#include "semantic_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// CONTEXT
// ============================================================

typedef struct plugin_ctx plugin_ctx;

/**
 * Initialize plugin context.
 * @param ctx_out Output pointer for context
 * @param argc    Argument count from main()
 * @param argv    Argument vector from main()
 * @return 0 on success, -1 on error
 */
int sdk_init(plugin_ctx **ctx_out, int argc, char **argv);

/**
 * Finalize plugin context and free resources.
 * @param ctx_ptr Pointer to context (set to NULL on return)
 */
void sdk_fini(plugin_ctx **ctx_ptr);

/**
 * Run plugin event loop. Blocks until shutdown.
 * @param ctx Plugin context
 * @return 0 on normal shutdown, -1 on error
 */
int sdk_run(plugin_ctx *ctx);

/**
 * Request plugin shutdown (thread-safe).
 */
void sdk_request_shutdown(plugin_ctx *ctx);

// ============================================================
// DATA REPORTING (IN Plugins)
// ============================================================

typedef struct sdk_metric sdk_metric;

/**
 * Create a new metric builder.
 * @param ctx Plugin context
 * @return Metric builder (must call sdk_metric_report() or sdk_metric_free())
 */
sdk_metric *sdk_metric_new(plugin_ctx *ctx);

/**
 * Set semantic type.
 * @return self for chaining
 */
sdk_metric *sdk_metric_semantic(sdk_metric *m, semantic_type type);

/**
 * Set numeric value.
 * @return self for chaining
 */
sdk_metric *sdk_metric_value(sdk_metric *m, double val);

/**
 * Set monetary value with currency.
 * @param val      Numeric value
 * @param currency ISO 4217 currency code (e.g., "SEK", "EUR")
 * @return self for chaining
 */
sdk_metric *sdk_metric_monetary(sdk_metric *m, double val, const char *currency);

/**
 * Set timestamp (optional, defaults to now).
 * @param ts Unix timestamp in seconds
 * @return self for chaining
 */
sdk_metric *sdk_metric_timestamp(sdk_metric *m, int64_t ts);

/**
 * Set floor constraint (optional).
 * Values below floor will be clamped.
 * @return self for chaining
 */
sdk_metric *sdk_metric_floor(sdk_metric *m, double floor);

/**
 * Set cap constraint (optional).
 * Values above cap will be clamped.
 * @return self for chaining
 */
sdk_metric *sdk_metric_cap(sdk_metric *m, double cap);

/**
 * Submit metric to Core and free builder.
 * @return 0 on success, -1 on error
 */
int sdk_metric_report(sdk_metric *m);

/**
 * Free metric builder without submitting.
 */
void sdk_metric_free(sdk_metric *m);

/**
 * Report raw extension data (Tier 2).
 * @param ctx  Plugin context
 * @param key  Dot-separated key (e.g., "vendor.sensor.reading")
 * @param json JSON payload
 * @param ts   Unix timestamp (0 = now)
 * @return 0 on success, -1 on error
 */
int sdk_report_raw(plugin_ctx *ctx, const char *key, const char *json, int64_t ts);

// ============================================================
// DATA QUERY (OUT Plugins)
// ============================================================

/**
 * Data point returned from queries.
 */
typedef struct {
    int64_t timestamp;
    double  value;
    char    currency[4];  // Empty string if not monetary
} sdk_data_point;

/**
 * Query most recent value for a semantic type.
 * @param ctx  Plugin context
 * @param type Semantic type to query
 * @param out  Output data point
 * @return 0 on success, -1 if not found or error
 */
int sdk_query_latest(plugin_ctx *ctx, semantic_type type, sdk_data_point *out);

/**
 * Query values in time range.
 * @param ctx      Plugin context
 * @param type     Semantic type to query
 * @param from_ts  Start timestamp (inclusive)
 * @param to_ts    End timestamp (inclusive)
 * @param out      Output array (caller must call sdk_free_points())
 * @param count    Output count
 * @return 0 on success, -1 on error
 */
int sdk_query_range(plugin_ctx *ctx, semantic_type type,
                    int64_t from_ts, int64_t to_ts,
                    sdk_data_point **out, size_t *count);

/**
 * Free data points returned by sdk_query_range().
 */
void sdk_free_points(sdk_data_point *points);

// ============================================================
// ENDPOINT REGISTRATION (OUT Plugins)
// ============================================================

/**
 * HTTP request (read-only).
 */
typedef struct sdk_request sdk_request;

/**
 * HTTP response (write).
 */
typedef struct sdk_response sdk_response;

/**
 * Get request method ("GET", "POST", etc.).
 */
const char *sdk_request_method(const sdk_request *req);

/**
 * Get request path ("/api/energy-strategy").
 */
const char *sdk_request_path(const sdk_request *req);

/**
 * Get raw query string ("foo=bar&baz=qux").
 */
const char *sdk_request_query(const sdk_request *req);

/**
 * Get request body (may be empty).
 */
const char *sdk_request_body(const sdk_request *req);

/**
 * Get request header by name (case-insensitive).
 * @return Header value or NULL if not present
 */
const char *sdk_request_header(const sdk_request *req, const char *name);

/**
 * Set response status code.
 */
void sdk_response_status(sdk_response *resp, int code);

/**
 * Set response header.
 */
void sdk_response_header(sdk_response *resp, const char *name, const char *value);

/**
 * Set JSON response body (also sets Content-Type).
 */
void sdk_response_json(sdk_response *resp, const char *json);

/**
 * Set raw response body.
 */
void sdk_response_body(sdk_response *resp, const char *data, size_t len);

/**
 * Handler function signature.
 * @param ctx  Plugin context
 * @param req  HTTP request
 * @param resp HTTP response (pre-initialized with 200 OK)
 * @return 0 on success, -1 on error (will return 500)
 */
typedef int (*sdk_handler_fn)(plugin_ctx *ctx, const sdk_request *req,
                               sdk_response *resp);

/**
 * Register HTTP endpoint.
 * @param ctx     Plugin context
 * @param method  HTTP method ("GET", "POST", etc.)
 * @param path    URL path ("/api/my-endpoint")
 * @param handler Handler function
 * @return 0 on success, -1 on error
 */
int sdk_register_endpoint(plugin_ctx *ctx, const char *method,
                          const char *path, sdk_handler_fn handler);

/**
 * Declare required semantic type dependency.
 * Core will reject plugin if no IN plugin provides this type.
 * @return 0 on success, -1 on error
 */
int sdk_require_semantic(plugin_ctx *ctx, semantic_type type);

/**
 * Declare optional semantic type dependency.
 * Plugin will start even if type is not available.
 * @return 0 on success, -1 on error
 */
int sdk_optional_semantic(plugin_ctx *ctx, semantic_type type);

// ============================================================
// LOGGING
// ============================================================

typedef enum {
    SDK_LOG_DEBUG,
    SDK_LOG_INFO,
    SDK_LOG_WARN,
    SDK_LOG_ERROR
} sdk_log_level;

/**
 * Log a message.
 * @param ctx   Plugin context
 * @param level Log level
 * @param fmt   printf-style format string
 */
void sdk_log(plugin_ctx *ctx, sdk_log_level level, const char *fmt, ...);

/**
 * Log a message (va_list version).
 */
void sdk_logv(plugin_ctx *ctx, sdk_log_level level, const char *fmt, va_list args);

// Convenience macros
#define SDK_DEBUG(ctx, ...) sdk_log(ctx, SDK_LOG_DEBUG, __VA_ARGS__)
#define SDK_INFO(ctx, ...)  sdk_log(ctx, SDK_LOG_INFO, __VA_ARGS__)
#define SDK_WARN(ctx, ...)  sdk_log(ctx, SDK_LOG_WARN, __VA_ARGS__)
#define SDK_ERROR(ctx, ...) sdk_log(ctx, SDK_LOG_ERROR, __VA_ARGS__)

// ============================================================
// TRIGGER CALLBACKS (Optional)
// ============================================================

/**
 * Trigger callback (for IN plugins).
 * Called when Core wants plugin to fetch/report data.
 */
typedef void (*sdk_trigger_fn)(plugin_ctx *ctx, const char *reason);

/**
 * Set trigger callback.
 * @param ctx Plugin context
 * @param fn  Callback function
 */
void sdk_set_trigger_callback(plugin_ctx *ctx, sdk_trigger_fn fn);

#ifdef __cplusplus
}
#endif

#endif /* HEIMWATT_SDK_H */
```

---

## Internal Modules

### sdk_core.h

Context structure and lifecycle.

```c
#ifndef SDK_CORE_H
#define SDK_CORE_H

#include "heimwatt_sdk.h"
#include "sdk_ipc.h"

// Internal context structure
struct plugin_ctx {
    char           *plugin_id;
    ipc_client     *ipc;
    sdk_trigger_fn  trigger_cb;
    
    // Registered endpoints
    struct {
        char           *method;
        char           *path;
        sdk_handler_fn  handler;
    } endpoints[32];
    size_t endpoint_count;
    
    // Required semantics
    semantic_type required[64];
    size_t        required_count;
    
    semantic_type optional[64];
    size_t        optional_count;
    
    // State
    volatile int running;
};

// Internal functions
int  sdk_core_init(plugin_ctx **ctx, int argc, char **argv);
void sdk_core_fini(plugin_ctx **ctx);
int  sdk_core_run(plugin_ctx *ctx);

#endif
```

### sdk_ipc.h

Plugin-side IPC client.

```c
#ifndef SDK_IPC_H
#define SDK_IPC_H

#include <stddef.h>

typedef struct ipc_client ipc_client;

// Connect to Core's IPC socket
int ipc_client_connect(ipc_client **client, const char *socket_path);

// Disconnect
void ipc_client_disconnect(ipc_client **client);

// Send message (blocks until sent)
int ipc_client_send(ipc_client *client, const char *msg, size_t len);

// Receive message (blocks until received or timeout)
// Caller frees *msg
int ipc_client_recv(ipc_client *client, char **msg, size_t *len, int timeout_ms);

// Get file descriptor (for poll/select)
int ipc_client_fd(const ipc_client *client);

#endif
```

### sdk_report.h

Metric builder implementation.

```c
#ifndef SDK_REPORT_H
#define SDK_REPORT_H

#include "heimwatt_sdk.h"

struct sdk_metric {
    plugin_ctx   *ctx;
    semantic_type type;
    double        value;
    char          currency[4];
    int64_t       timestamp;
    double        floor_val;
    double        cap_val;
    int           has_floor;
    int           has_cap;
    int           is_monetary;
};

// Validate metric before sending
int sdk_metric_validate(const sdk_metric *m);

// Serialize to IPC message
char *sdk_metric_to_json(const sdk_metric *m);

#endif
```

### sdk_query.h

Query implementation.

```c
#ifndef SDK_QUERY_H
#define SDK_QUERY_H

#include "heimwatt_sdk.h"

// Send query, wait for response, parse result
int sdk_query_send_latest(plugin_ctx *ctx, semantic_type type, 
                          sdk_data_point *out);

int sdk_query_send_range(plugin_ctx *ctx, semantic_type type,
                         int64_t from_ts, int64_t to_ts,
                         sdk_data_point **out, size_t *count);

#endif
```

### sdk_endpoint.h

Endpoint registration and dispatch.

```c
#ifndef SDK_ENDPOINT_H
#define SDK_ENDPOINT_H

#include "heimwatt_sdk.h"

// Register endpoint with Core
int sdk_endpoint_register(plugin_ctx *ctx, const char *method, const char *path);

// Handle incoming HTTP_REQUEST message
int sdk_endpoint_dispatch(plugin_ctx *ctx, const char *request_json);

#endif
```

---

## Usage Examples

### IN Plugin: Weather Data

```c
#include <heimwatt_sdk.h>
#include <curl/curl.h>

static void fetch_and_report(plugin_ctx *ctx) {
    // Fetch from API
    double temp = fetch_temperature_from_api();
    double humidity = fetch_humidity_from_api();
    
    // Report temperature
    sdk_metric_new(ctx)
        ->semantic(SEM_ATMOSPHERE_TEMPERATURE)
        ->value(temp)
        ->floor(-50.0)
        ->cap(60.0)
        ->report();
    
    // Report humidity
    sdk_metric_new(ctx)
        ->semantic(SEM_ATMOSPHERE_HUMIDITY)
        ->value(humidity)
        ->floor(0.0)
        ->cap(100.0)
        ->report();
}

static void on_trigger(plugin_ctx *ctx, const char *reason) {
    SDK_INFO(ctx, "Trigger received: %s", reason);
    fetch_and_report(ctx);
}

int main(int argc, char **argv) {
    plugin_ctx *ctx;
    if (sdk_init(&ctx, argc, argv) != 0) {
        return 1;
    }
    
    sdk_set_trigger_callback(ctx, on_trigger);
    
    int ret = sdk_run(ctx);
    sdk_fini(&ctx);
    return ret;
}
```

### OUT Plugin: Energy Strategy

```c
#include <heimwatt_sdk.h>
#include "lps.h"

static int handle_strategy(plugin_ctx *ctx, const sdk_request *req,
                            sdk_response *resp) {
    // Get query params
    const char *query = sdk_request_query(req);
    int horizon = parse_horizon(query);  // Default 48
    
    // Query price data
    sdk_data_point *prices;
    size_t price_count;
    int64_t now = time(NULL);
    
    if (sdk_query_range(ctx, SEM_ENERGY_PRICE_SPOT, now, 
                        now + horizon * 3600, &prices, &price_count) != 0) {
        sdk_response_status(resp, 500);
        sdk_response_json(resp, "{\"error\":\"Failed to query prices\"}");
        return -1;
    }
    
    // Query battery SOC
    sdk_data_point soc;
    sdk_query_latest(ctx, SEM_STORAGE_SOC, &soc);
    
    // Build LPS problem
    lps_problem problem = build_problem(prices, price_count, soc.value);
    lps_solution solution;
    lps_solve(&problem, &solution);
    
    // Build response
    char *json = solution_to_json(&solution);
    sdk_response_json(resp, json);
    free(json);
    
    sdk_free_points(prices);
    lps_solution_destroy(&solution);
    
    return 0;
}

int main(int argc, char **argv) {
    plugin_ctx *ctx;
    sdk_init(&ctx, argc, argv);
    
    sdk_require_semantic(ctx, SEM_ENERGY_PRICE_SPOT);
    sdk_require_semantic(ctx, SEM_STORAGE_SOC);
    sdk_optional_semantic(ctx, SEM_SOLAR_GHI);
    
    sdk_register_endpoint(ctx, "GET", "/api/energy-strategy", handle_strategy);
    
    sdk_run(ctx);
    sdk_fini(&ctx);
    return 0;
}
```

---

## Building Plugins

Plugins link against `libheimwatt-sdk.so`:

```makefile
# Plugin Makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I/usr/include/heimwatt
LDFLAGS = -lheimwatt-sdk

TARGET = my_plugin

$(TARGET): my_plugin.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

---

## Error Handling

All SDK functions return:
- `0` on success
- `-1` on error

Use `SDK_ERROR()` to log errors before returning.

---

> **Document Map**:
> - [Architecture Overview](../architecture.md)
> - [Plugin System](../plugins/design.md)
> - [Semantic Types](../../semantic_types_reference.md)
