# API Reference

> **Version**: 2.0 (2026-01-15)

This document lists all public APIs organized by module.

---

## Conventions

All functions follow the coding standard:
- **Success**: Returns `0`
- **Failure**: Returns negative errno (`-EINVAL`, `-ENOMEM`, etc.)
- **Lifecycle**: `create/destroy` (heap), `init/fini` (stack), `open/close` (resources)
- **Destructors**: Take `T**` pointer and set to `NULL`

---

## Core Lifecycle (`server.h`)

```c
heimwatt_ctx* heimwatt_create(void);
int  heimwatt_init(heimwatt_ctx* ctx, const char* config_path);
void heimwatt_run(heimwatt_ctx* ctx);
void heimwatt_request_shutdown(heimwatt_ctx* ctx);  // Thread-safe
void heimwatt_destroy(heimwatt_ctx** ctx_ptr);
bool heimwatt_is_running(const heimwatt_ctx* ctx);  // Thread-safe
```

---

## Database (`db.h`)

Backend-agnostic interface. Implementation selected at compile time.

### Lifecycle

```c
int  db_open(db_handle** db, const char* path);
void db_close(db_handle** db);
const char* db_error_message(const db_handle* db);
```

### Tier 1: Semantic Data

```c
int db_insert_tier1(db_handle* db, semantic_type type, int64_t timestamp,
                    double value, const char* currency, const char* source_id);

int db_query_latest_tier1(db_handle* db, semantic_type type,
                          double* out_val, int64_t* out_ts);

int db_query_range_tier1(db_handle* db, semantic_type type,
                         int64_t from_ts, int64_t to_ts,
                         double** out_values, int64_t** out_ts, size_t* out_count);
```

### Tier 2: Raw Extension Data

```c
int db_insert_tier2(db_handle* db, const char* key, int64_t timestamp,
                    const char* json_payload, const char* source_id);

int db_query_latest_tier2(db_handle* db, const char* key,
                          char** out_json, int64_t* out_ts);
```

### Maintenance

```c
void db_free(void* ptr);  // Free query results
int  db_prune_tier1(db_handle* db, semantic_type type, int64_t before_ts);
int  db_maintenance(db_handle* db);  // Vacuum/optimize
```

---

## Plugin SDK (`heimwatt_sdk.h`)

### Lifecycle

```c
int  sdk_create(plugin_ctx** ctx_out, int argc, char** argv);
int  sdk_run(plugin_ctx* ctx);
void sdk_destroy(plugin_ctx** ctx_ptr);
void sdk_log(const plugin_ctx* ctx, sdk_log_level level, const char* fmt, ...);
```

### Data Reporting (IN Plugins)

```c
typedef struct {
    semantic_type semantic;   // Required
    double value;             // Required (canonical unit)
    int64_t timestamp;        // Optional (0 = now)
    const char* currency;     // Required for monetary types
    double floor_val;         // Optional constraint
    bool has_floor;
    double cap_val;           // Optional constraint
    bool has_cap;
} sdk_metric;

int sdk_report(plugin_ctx* ctx, const sdk_metric* metric);
int sdk_report_raw(plugin_ctx* ctx, const char* key, int64_t timestamp,
                   const char* json_fmt, ...);
```

### Data Querying (OUT Plugins)

```c
typedef struct {
    int64_t timestamp;
    double value;
    char currency[4];
} sdk_data_point;

int  sdk_query_latest(plugin_ctx* ctx, semantic_type type, sdk_data_point* out);
int  sdk_query_history(plugin_ctx* ctx, semantic_type type,
                       int64_t from_ts, int64_t to_ts,
                       sdk_data_point** out_array, size_t* out_count);
void sdk_data_point_destroy(sdk_data_point** points_ptr);
```

### Endpoint Registration (OUT Plugins)

```c
typedef int (*sdk_api_handler)(plugin_ctx* ctx, const sdk_req* req, sdk_resp* resp);

int sdk_register_endpoint(plugin_ctx* ctx, const char* method,
                          const char* path, sdk_api_handler handler);
int sdk_require_semantic(plugin_ctx* ctx, semantic_type type);
```

### Response Helpers

```c
void sdk_resp_set_status(sdk_resp* resp, int code);
void sdk_resp_set_json(sdk_resp* resp, const char* json_body);
void sdk_resp_set_header(sdk_resp* resp, const char* key, const char* val);
```

---

## Semantic Types (`semantic_types.h`)

```c
typedef enum {
    SEM_UNKNOWN = 0,
    SEM_ATMOSPHERE_TEMPERATURE,
    SEM_ATMOSPHERE_HUMIDITY,
    SEM_ENERGY_PRICE_SPOT,
    SEM_STORAGE_SOC,
    // ... ~100 types defined
    SEM_TYPE_COUNT
} semantic_type;

const semantic_meta* semantic_get_meta(semantic_type type);
semantic_type semantic_from_string(const char* id);
```

---

## Version (`version.h`)

```c
#define HEIMWATT_VERSION_MAJOR 0
#define HEIMWATT_VERSION_MINOR 1
#define HEIMWATT_VERSION_PATCH 0
#define HEIMWATT_VERSION_STRING "0.1.0"

#define HEIMWATT_IPC_VERSION 1
#define HEIMWATT_IPC_VERSION_MIN 1
```
