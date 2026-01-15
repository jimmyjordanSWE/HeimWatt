# HeimWatt SDK Specification

> **Status**: FINAL
> **Version**: 3.0
> **Date**: 2026-01-15

## 1. Overview

The HeimWatt SDK provides a C interface for plugins to interact with HeimWatt Core. It abstracts IPC, scheduling, HTTP, JSON, and state management so plugins can focus on one thing: **mapping vendor data to semantic types**.

## 2. Design Philosophy

| Layer | Responsibility |
|-------|---------------|
| **manifest.json** | What to fetch, when, auth. No recompile needed. |
| **Plugin C Code** | Map vendor fields → semantic types (human logic). |
| **SDK** | Everything else: HTTP, JSON, scheduling, reporting. |

**Key Principle**: Plugin code should be 90% mapping logic. All infrastructure is SDK's job.

## 3. Semantic Types

Semantic types are defined via **X-macros** in `semantic_types.h` (shared between Core and SDK).

- **Compile-time**: Adding new types requires rebuild + restart (~1 second).
- **String Lookup**: Plugins use `sdk_type_lookup()` to get type IDs by name.
- **Validation**: Core validates values against min/max defined in X-macro metadata.

## 4. C API Reference

### 4.1 Lifecycle
```c
int sdk_create(plugin_ctx** ctx_out, int argc, char** argv);
int sdk_run(plugin_ctx* ctx);
void sdk_destroy(plugin_ctx** ctx_ptr);
```

### 4.2 Configuration
```c
// Get config value from manifest. Caller must free.
int sdk_get_config(plugin_ctx* ctx, const char* key, char** value);
```

### 4.3 Scheduling

#### Interval (relative to start)
```c
typedef void (*sdk_tick_handler)(plugin_ctx* ctx, int64_t timestamp);
int sdk_register_ticker(plugin_ctx* ctx, sdk_tick_handler handler);
```

#### Cron (wall-clock aligned)
```c
int sdk_register_cron(plugin_ctx* ctx, const char* expr, sdk_tick_handler handler);
```

#### File Descriptor (event-driven)
```c
typedef void (*sdk_io_handler)(plugin_ctx* ctx, int fd);
int sdk_register_fd(plugin_ctx* ctx, int fd, sdk_io_handler handler);
```

### 4.4 HTTP
```c
// Simple GET, returns HTTP status or -1. Caller frees body.
int sdk_http_get(plugin_ctx* ctx, const char* url, char** body_out);

// Fetch + parse JSON in one call.
int sdk_fetch_json(plugin_ctx* ctx, const char* url, json_value** out);
```

### 4.5 JSON Utilities
```c
typedef struct json_value json_value;

json_value* sdk_json_parse(const char* str);
void sdk_json_free(json_value* v);

// Navigation
const json_value* sdk_json_get(const json_value* obj, const char* key);
size_t sdk_json_array_size(const json_value* arr);
const json_value* sdk_json_array_get(const json_value* arr, size_t idx);

// Extraction
const char* sdk_json_string(const json_value* v);
double sdk_json_number(const json_value* v);
int64_t sdk_json_int(const json_value* v);
```

### 4.6 Time Series Iteration (Option B Pattern)
```c
// Parse ISO8601 timestamp
int64_t sdk_time_parse_iso(const char* str);

// Macro for iterating time series arrays
#define sdk_foreach_ts(arr, entry, ts) \
    for (size_t _i = 0; _i < sdk_json_array_size(arr) && \
         ((entry) = sdk_json_array_get(arr, _i), \
          (ts) = sdk_time_parse_iso(sdk_json_string(sdk_json_get(entry, "validTime"))), 1); \
         _i++)
```

### 4.7 Reporting
```c
// Full control
int sdk_report(plugin_ctx* ctx, const sdk_metric* metric);

// Shorthand - string type name, Core looks up ID
int sdk_report_value(plugin_ctx* ctx, const char* type_name, double value, int64_t ts);

// With currency (for price types)
int sdk_report_price(plugin_ctx* ctx, const char* type_name, double value, 
                     const char* currency, int64_t ts);

// Type lookup (cache result for performance)
semantic_type sdk_type_lookup(plugin_ctx* ctx, const char* name);
```

### 4.8 State Persistence
```c
int sdk_state_save(plugin_ctx* ctx, const char* key, const char* value);
int sdk_state_load(plugin_ctx* ctx, const char* key, char** value_out);
```

### 4.9 Logging
```c
void sdk_log(plugin_ctx* ctx, sdk_log_level level, const char* fmt, ...);
```

### 4.10 Boilerplate Macro
```c
#define HEIMWATT_PLUGIN_ENTRY(init_func) \
    int main(int argc, char** argv) { \
        plugin_ctx* ctx = NULL; \
        if (sdk_create(&ctx, argc, argv) < 0) return 1; \
        if (init_func(ctx) < 0) { sdk_destroy(&ctx); return 1; } \
        int ret = sdk_run(ctx); \
        sdk_destroy(&ctx); \
        return ret; \
    }
```

## 5. Example Plugin (SMHI Weather)

```c
#include <heimwatt_sdk.h>

void on_tick(plugin_ctx* ctx, int64_t now) {
    char* url = NULL;
    sdk_get_config(ctx, "url", &url);
    
    json_value* data = NULL;
    if (sdk_fetch_json(ctx, url, &data) < 0) {
        sdk_log(ctx, SDK_LOG_ERROR, "Fetch failed");
        free(url);
        return;
    }
    
    json_value* series = sdk_json_get(data, "timeSeries");
    const json_value* entry;
    int64_t ts;
    
    sdk_foreach_ts(series, entry, ts) {
        json_value* params = sdk_json_get(entry, "parameters");
        for (size_t i = 0; i < sdk_json_array_size(params); i++) {
            json_value* p = sdk_json_array_get(params, i);
            const char* name = sdk_json_string(sdk_json_get(p, "name"));
            double val = sdk_json_number(sdk_json_array_get(sdk_json_get(p, "values"), 0));
            
            // The mapping (human work)
            if (strcmp(name, "t") == 0)
                sdk_report_value(ctx, "ATMOSPHERE_TEMPERATURE", val, ts);
            else if (strcmp(name, "r") == 0)
                sdk_report_value(ctx, "ATMOSPHERE_HUMIDITY", val, ts);
        }
    }
    
    sdk_json_free(data);
    free(url);
}

int init(plugin_ctx* ctx) {
    sdk_register_cron(ctx, "15 * * * *", on_tick);  // Every hour at :15
    return 0;
}

HEIMWATT_PLUGIN_ENTRY(init)
```

## 6. Security

### HTTP/TLS Policy

The SDK **enforces HTTPS by default**. Flags in `manifest.json` control exceptions:

| Flag | Default | Scope |
|------|---------|-------|
| `allow_http_localhost` | `true` | HTTP for `localhost`, `127.0.0.1`, `::1` |
| `allow_http_lan` | `false` | HTTP for private IPs (`192.168.*`, `10.*`, `172.16-31.*`) |
| `allow_http_public` | `false` | HTTP for any URL (dangerous!) |
| `allow_self_signed` | `false` | Accept self-signed TLS certificates |

**Resolution order**: localhost → LAN → public. First match wins.

**Example manifest.json:**
```json
{
  "id": "com.example.local-inverter",
  "security": {
    "allow_http_lan": true,
    "allow_self_signed": true
  }
}
```

### Use Case Matrix

| Target | Required Flags |
|--------|---------------|
| `https://api.smhi.se` | None (default) |
| `http://localhost:8080` | None (default allows localhost) |
| `http://192.168.1.50/sensor` | `allow_http_lan: true` |
| `https://192.168.1.50` (self-signed) | `allow_self_signed: true` |
| `http://example.com` | `allow_http_public: true` (avoid!) |

## 7. Deployment


See [DEPLOYMENT.md](DEPLOYMENT.md) for Docker-based distribution and automatic updates.
