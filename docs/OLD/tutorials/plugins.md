# Plugin Development Tutorial

This guide covers how to write, build, and run plugins for HeimWatt.

## 1. Writing an IN Plugin (Data Source)

**Scenario**: Fetch weather data from an external API (SMHI) and report it to Core.

```c
#include <heimwatt_sdk.h>

// Called when TRIGGER received (e.g., every hour)
void on_trigger(plugin_ctx *ctx, const char *reason) {
    // 1. Fetch data from external API
    // double temp_c = fetch_smhi_temperature();
    double temp_c = 15.5; // Mock value
    
    // 2. Report to Core using Builder pattern
    sdk_metric_new(ctx)
        ->semantic(SEM_ATMOSPHERE_TEMPERATURE)
        ->value(temp_c)
        ->floor(-50.0) // Constraint: Ignore values < -50
        ->cap(60.0)    // Constraint: Ignore values > 60
        ->report();
}

int main(int argc, char **argv) {
    plugin_ctx *ctx;
    
    // Initialize SDK
    if (sdk_init(&ctx, argc, argv) != 0) {
        return 1;
    }
    
    // Register trigger callback
    sdk_set_trigger_callback(ctx, on_trigger);
    
    // Enter event loop (blocks until shutdown)
    int ret = sdk_run(ctx);
    
    // Cleanup
    sdk_fini(&ctx);
    return ret;
}
```

## 2. Writing an OUT Plugin (Calculator)

**Scenario**: Calculate an energy strategy based on spot prices and battery level.

```c
#include <heimwatt_sdk.h>
#include "lps.h" // Hypothetical Linear Programming Solver

// Handler for GET /api/energy-strategy
static int handle_strategy(plugin_ctx *ctx, const sdk_request *req, 
                           sdk_response *resp) {
    // 1. Parse query parameters
    const char *query = sdk_request_query(req);
    int horizon = 48; // Default horizon
    
    // 2. Query data dependencies from Core
    sdk_data_point *prices;
    size_t count;
    int64_t now = time(NULL);
    
    // Get 48 hours of spot prices
    if (sdk_query_range(ctx, SEM_ENERGY_PRICE_SPOT, now, now + 48*3600, 
                        &prices, &count) != 0) {
        sdk_response_status(resp, 500);
        sdk_response_json(resp, "{\"error\":\"Failed to query prices\"}");
        return -1;
    }
    
    // Get current battery SOC
    sdk_data_point soc;
    sdk_query_latest(ctx, SEM_STORAGE_SOC, &soc);
    
    // 3. Perform computation
    // lps_problem problem = build_problem(prices, count, soc.value);
    // lps_solution solution;
    // lps_solve(&problem, &solution);
    
    // 4. Send response
    // char *json = solution_to_json(&solution);
    const char *json = "{\"action\":\"charge\",\"amount\":5.0}";
    sdk_response_json(resp, json);
    
    // 5. Cleanup query results
    sdk_free_points(prices);
    return 0;
}

int main(int argc, char **argv) {
    plugin_ctx *ctx;
    sdk_init(&ctx, argc, argv);
    
    // Declare dependencies (Core verifies these exist)
    sdk_require_semantic(ctx, SEM_ENERGY_PRICE_SPOT);
    sdk_require_semantic(ctx, SEM_STORAGE_SOC);
    
    // Register HTTP endpoint
    sdk_register_endpoint(ctx, "GET", "/api/energy-strategy", handle_strategy);
    
    sdk_run(ctx);
    sdk_fini(&ctx);
    return 0;
}
```

## 3. Building Plugins

Plugins are standard executables that link against `libheimwatt-sdk.so`.

**Makefile**:
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I/usr/include/heimwatt
LDFLAGS = -lheimwatt-sdk

TARGET = my_plugin

$(TARGET): my_plugin.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
```
