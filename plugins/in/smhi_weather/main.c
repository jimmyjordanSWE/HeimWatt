#include <heimwatt_sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "memory.h"

// Semantic Type Names
#define TYPE_TEMP "atmosphere.temperature"
#define TYPE_HUMID "atmosphere.humidity"
#define TYPE_WIND "atmosphere.wind_speed"

static bool history_fetched = false;

// ----------------------------------------------------------------------------
// URL Fetching Helper
// ----------------------------------------------------------------------------
static char *get_config_url(plugin_ctx *ctx, const char *key, const char *default_val)
{
    char *val = NULL;
    if (sdk_get_config(ctx, key, &val) == 0 && val)
    {
        return val;
    }
    return default_val ? mem_strdup(default_val) : NULL;
}

// ----------------------------------------------------------------------------
// History Fetch (Observations)
// ----------------------------------------------------------------------------
static void fetch_obs(plugin_ctx *ctx, const char *config_key, const char *type_name)
{
    char *url = get_config_url(ctx, config_key, NULL);
    if (!url)
    {
        sdk_log(ctx, SDK_LOG_WARN, "No history URL configured for %s (key: %s)", type_name,
                config_key);
        return;
    }

    sdk_log(ctx, SDK_LOG_INFO, "Fetching history for %s...", type_name);

    json_value *data = NULL;
    if (sdk_fetch_json(ctx, url, &data) < 0)
    {
        sdk_log(ctx, SDK_LOG_ERROR, "Failed to fetch history for %s", type_name);
        mem_free(url);
        return;
    }

    // Expected format: { "value": [ ... ] }
    json_value *values = (json_value *) sdk_json_get(data, "value");
    if (!values)
    {
        sdk_log(ctx, SDK_LOG_WARN, "No 'value' array in obs response");
        sdk_json_free(data);
        mem_free(url);
        return;
    }

    size_t count = sdk_json_array_size(values);
    size_t reported = 0;

    for (size_t i = 0; i < count; i++)
    {
        json_value *item = (json_value *) sdk_json_array_get(values, i);

        // "date": 1563453450000 (ms)
        int64_t ts_ms = sdk_json_int(sdk_json_get(item, "date"));
        int64_t ts = ts_ms / 1000;

        // "value": "12.3"
        const char *val_str = sdk_json_string(sdk_json_get(item, "value"));
        if (val_str)
        {
            double val = atof(val_str);
            sdk_report_value(ctx, type_name, val, ts);
            reported++;
        }
    }

    sdk_log(ctx, SDK_LOG_INFO, "Reported %zu history points for %s", reported, type_name);

    sdk_json_free(data);
    mem_free(url);
}

// ----------------------------------------------------------------------------
// Main Tick Handler
// ----------------------------------------------------------------------------
static void on_tick(plugin_ctx *ctx, int64_t now)
{
    int64_t real_now = time(NULL);

    // --- Backfill Mode (Run once on startup) ---
    if (!history_fetched)
    {
        sdk_log(ctx, SDK_LOG_INFO, "Triggering one-time history fetch...");
        // Look for keys like "url_obs_temp", "url_obs_humid"
        fetch_obs(ctx, "url_history_temp", TYPE_TEMP);
        fetch_obs(ctx, "url_history_humid", TYPE_HUMID);
        history_fetched = true;
    }

    // Continue to Realtime...
    if (now < real_now - 3600) return;  // Still skip realtime if way back in past (safety)

    // --- Realtime / Forecast Mode ---
    sdk_log(ctx, SDK_LOG_INFO, "Running tick - Forecast Mode (now=%ld)", now);
    char *url = get_config_url(ctx, "url_forecast", NULL);
    if (!url)
    {
        sdk_log(ctx, SDK_LOG_ERROR, "Missing 'url_forecast' config");
        return;
    }

    sdk_log(ctx, SDK_LOG_INFO, "Fetching forecast from: %s", url);
    json_value *data = NULL;
    if (sdk_fetch_json(ctx, url, &data) < 0)
    {
        sdk_log(ctx, SDK_LOG_ERROR, "Failed to fetch weather forecast from API");
        mem_free(url);
        return;
    }
    sdk_log(ctx, SDK_LOG_INFO, "Successfully fetched forecast data");

    json_value *series = (json_value *) sdk_json_get(data, "timeSeries");
    const json_value *entry;
    int64_t ts;

    sdk_foreach_ts(series, entry, ts)
    {
        if (ts < now) continue;

        json_value *params = (json_value *) sdk_json_get(entry, "parameters");
        size_t param_count = sdk_json_array_size(params);

        for (size_t i = 0; i < param_count; i++)
        {
            json_value *p = (json_value *) sdk_json_array_get(params, i);
            const char *name = sdk_json_string(sdk_json_get(p, "name"));
            json_value *vals = (json_value *) sdk_json_get(p, "values");
            double val = sdk_json_number(sdk_json_array_get(vals, 0));

            if (strcmp(name, "t") == 0)
            {
                sdk_report_value(ctx, TYPE_TEMP, val, ts);
            }
            else if (strcmp(name, "r") == 0)
            {
                sdk_report_value(ctx, TYPE_HUMID, val, ts);
            }
            else if (strcmp(name, "ws") == 0)
            {
                sdk_report_value(ctx, TYPE_WIND, val, ts);
            }
        }
    }

    sdk_json_free(data);
    mem_free(url);
}

int init(plugin_ctx *ctx)
{
    // Use ticker for Alpha/Debug (runs every 60s)
    // TODO: Switch back to Cron "15 * * * *" when scheduler is ready
    return sdk_register_ticker(ctx, on_tick);
}

HEIMWATT_PLUGIN_ENTRY(init)
