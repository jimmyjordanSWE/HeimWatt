# Plugin Development Guide

> **Version**: 2.0 (2026-01-15)

---

## Plugin Types

| Type | Purpose | Examples |
|------|---------|----------|
| **IN** | Fetch data, report to Core | Weather API, price feeds, sensors |
| **OUT** | Compute results, serve via API | Energy optimizer, alerts |

---

## Project Structure

```
plugins/
├── in/
│   └── my_weather/
│       ├── manifest.json
│       ├── main.c
│       └── Makefile
│
└── out/
    └── my_solver/
        ├── manifest.json
        ├── main.c
        └── Makefile
```

---

## Manifest Schema

### IN Plugin

```json
{
  "type": "in",
  "id": "com.example.my-weather",
  "name": "My Weather Plugin",
  "version": "1.0.0",
  "provides": {
    "known": ["ATMOSPHERE_TEMPERATURE", "ATMOSPHERE_HUMIDITY"],
    "raw": ["myweather.uv_index"]
  },
  "schedule": {
    "interval_seconds": 3600
  }
}
```

### OUT Plugin

```json
{
  "type": "out",
  "id": "com.example.my-solver",
  "name": "My Solver Plugin",
  "version": "1.0.0",
  "requires": {
    "known": ["ENERGY_PRICE_SPOT", "STORAGE_SOC"]
  },
  "endpoints": [
    { "method": "GET", "path": "/api/my-solver/plan" }
  ]
}
```

---

## IN Plugin Template

```c
#include <heimwatt_sdk.h>
#include <errno.h>

static int fetch_and_report(plugin_ctx* ctx) {
    // Fetch from external API...
    char* api_key = NULL;
    if (sdk_get_config(ctx, "api_key", &api_key) < 0) {
        sdk_log(ctx, SDK_LOG_ERROR, "Missing api_key config");
        return -1;
    }

    double temperature = 22.5; // In reality, use api_key to fetch
    free(api_key); // Remember to free config strings!

    
    sdk_metric metric = {
        .semantic = SEM_ATMOSPHERE_TEMPERATURE,
        .value = temperature,  // Celsius (canonical unit)
        .timestamp = 0         // 0 = now
    };
    
    return sdk_report(ctx, &metric);
}

int main(int argc, char** argv) {
    plugin_ctx* ctx = NULL;
    
    int ret = sdk_create(&ctx, argc, argv);
    if (ret < 0) return 1;
    
    // Report initial data
    fetch_and_report(ctx);
    
    // Run event loop (handles scheduling)
    ret = sdk_run(ctx);
    
    sdk_destroy(&ctx);
    return ret < 0 ? 1 : 0;
}
```

---

## OUT Plugin Template

```c
#include <heimwatt_sdk.h>
#include <errno.h>
#include <string.h>

static int handle_plan(plugin_ctx* ctx, const sdk_req* req, sdk_resp* resp) {
    // Query latest data
    sdk_data_point price;
    if (sdk_query_latest(ctx, SEM_ENERGY_PRICE_SPOT, &price) < 0) {
        sdk_resp_set_status(resp, 500);
        sdk_resp_set_json(resp, "{\"error\":\"No price data\"}");
        return 0;
    }
    
    // Compute result...
    char json[256];
    snprintf(json, sizeof(json),
             "{\"price\":%.2f,\"currency\":\"%s\",\"action\":\"charge\"}",
             price.value, price.currency);
    
    sdk_resp_set_status(resp, 200);
    sdk_resp_set_json(resp, json);
    return 0;
}

int main(int argc, char** argv) {
    plugin_ctx* ctx = NULL;
    
    int ret = sdk_create(&ctx, argc, argv);
    if (ret < 0) return 1;
    
    // Declare dependencies
    sdk_require_semantic(ctx, SEM_ENERGY_PRICE_SPOT);
    
    // Register endpoint
    sdk_register_endpoint(ctx, "GET", "/api/my-solver/plan", handle_plan);
    
    // Run event loop
    ret = sdk_run(ctx);
    
    sdk_destroy(&ctx);
    return ret < 0 ? 1 : 0;
}
```

---

## Semantic Types

Use canonical SI units. No conversion needed.

| Type | Unit | Example |
|------|------|---------|
| `SEM_ATMOSPHERE_TEMPERATURE` | Celsius | 22.5 |
| `SEM_ATMOSPHERE_PRESSURE` | hPa | 1013.25 |
| `SEM_ENERGY_PRICE_SPOT` | currency/kWh | 0.85 + "SEK" |
| `SEM_STORAGE_SOC` | percent | 75.0 |
| `SEM_SOLAR_GHI` | W/m² | 850.0 |

See `include/semantic_types.h` for complete list (~100 types).

---

## Error Handling

Follow the coding standard:

```c
int ret = sdk_query_latest(ctx, SEM_ENERGY_PRICE_SPOT, &price);
if (ret < 0) {
    sdk_log(ctx, SDK_LOG_ERROR, "Query failed: %s", strerror(-ret));
    return ret;
}
```

---

## Logging

```c
sdk_log(ctx, SDK_LOG_DEBUG, "Fetching data...");
sdk_log(ctx, SDK_LOG_INFO, "Reported temperature: %.1f", temp);
sdk_log(ctx, SDK_LOG_WARN, "API rate limited, backing off");
sdk_log(ctx, SDK_LOG_ERROR, "Failed to connect: %s", strerror(errno));
```

---

## Building

```makefile
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -I../../include
LDFLAGS = -L../../build -lheimwatt-sdk

my_plugin: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

---

## Testing

1. Start Core: `./heimwatt`
2. Check plugin loaded: `curl http://localhost:8080/api/status`
3. Test endpoint: `curl http://localhost:8080/api/my-solver/plan`
4. Check logs: `tail -f logs/plugins/com.example.my-solver.log`
