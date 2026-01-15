#define _GNU_SOURCE
#include <heimwatt_sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Semantic Type Names (String based lookup)
#define TYPE_TEMP "ATMOSPHERE_TEMPERATURE"
#define TYPE_HUMID "ATMOSPHERE_HUMIDITY"
#define TYPE_WIND "WIND_SPEED"

// Default SMHI URL (Stockholm)
// In real life, 'lat' and 'lon' would inject into this URL string.
// For simplicity of prototype, hardcoded or fetched full URL from config.
#define DEFAULT_URL                                                                             \
    "https://opendata-download-metfcst.smhi.se/api/category/pmp3g/version/2/geotype/point/lon/" \
    "18.0686/lat/59.3293/data.json"

static void on_tick(plugin_ctx* ctx, int64_t now) {
    char* url = NULL;
    if (sdk_get_config(ctx, "url", &url) < 0) {
        // Fallback
        url = strdup(DEFAULT_URL);
    }

    sdk_log(ctx, SDK_LOG_INFO, "Fetching weather data from SMHI...");

    json_value* data = NULL;
    if (sdk_fetch_json(ctx, url, &data) < 0) {
        sdk_log(ctx, SDK_LOG_ERROR, "Failed to fetch weather data");
        free(url);
        return;
    }

    json_value* series = (json_value*)sdk_json_get(data, "timeSeries");
    if (!series) {
        sdk_log(ctx, SDK_LOG_WARN, "No timeSeries in response");
        sdk_json_free(data);
        free(url);
        return;
    }

    const json_value* entry;
    int64_t ts;

    // Use the TS iteration macro
    sdk_foreach_ts(series, entry, ts) {
        if (ts < now) continue;  // Skip past

        json_value* params = (json_value*)sdk_json_get(entry, "parameters");
        size_t param_count = sdk_json_array_size(params);

        for (size_t i = 0; i < param_count; i++) {
            json_value* p = (json_value*)sdk_json_array_get(params, i);
            const char* name = sdk_json_string(sdk_json_get(p, "name"));
            json_value* vals = (json_value*)sdk_json_get(p, "values");
            double val = sdk_json_number(sdk_json_array_get(vals, 0));

            // Mapping Logic (The Human Part)
            if (strcmp(name, "t") == 0) {
                sdk_report_value(ctx, TYPE_TEMP, val, ts);
            } else if (strcmp(name, "r") == 0) {
                sdk_report_value(ctx, TYPE_HUMID, val, ts);
            } else if (strcmp(name, "ws") == 0) {
                sdk_report_value(ctx, TYPE_WIND, val, ts);
            }
        }
    }

    sdk_json_free(data);
    free(url);
    sdk_log(ctx, SDK_LOG_INFO, "Weather data reported successfully");
}

int init(plugin_ctx* ctx) {
    // Register cron: Every hour at :15
    return sdk_register_cron(ctx, "15 * * * *", on_tick);
}

HEIMWATT_PLUGIN_ENTRY(init)
