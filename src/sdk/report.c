#include <heimwatt_sdk.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdk_internal.h"

int sdk_report(plugin_ctx* ctx, const sdk_metric* metric) {
    if (!ctx || !metric) return -1;

    char buf[1024];
    // Format: {"cmd":"report", "semantic":ID, "val":123.4, "ts":123456789, "curr":"SEK"}

    char currency_part[64] = "";
    if (metric->currency) {
        snprintf(currency_part, sizeof(currency_part), ", \"curr\":\"%s\"", metric->currency);
    }

    snprintf(buf, sizeof(buf), "{\"cmd\":\"report\", \"sem\":%d, \"val\":%f, \"ts\":%ld%s}",
             metric->semantic, metric->value, metric->timestamp, currency_part);

    return sdk_ipc_send(ctx, buf);
}

int sdk_report_value(plugin_ctx* ctx, const char* type_name, double value, int64_t ts) {
    semantic_type id = sdk_type_lookup(ctx, type_name);
    if (id < 0) return -1;  // Unknown type

    sdk_metric m = {.semantic = id, .value = value, .timestamp = ts, .currency = NULL};
    return sdk_report(ctx, &m);
}

int sdk_report_price(plugin_ctx* ctx, const char* type_name, double value, const char* currency,
                     int64_t ts) {
    semantic_type id = sdk_type_lookup(ctx, type_name);
    if (id < 0) return -1;

    sdk_metric m = {.semantic = id, .value = value, .timestamp = ts, .currency = currency};
    return sdk_report(ctx, &m);
}

// Very simple cache for type lookup
// For MVP, just linear search or direct query every time?
// Optimization: simple static cache?
// Problem: different contexts/plugins sharing static? SDK is linked into plugin so static is
// per-plugin process. Safe.
#define MAX_CACHE 64
static struct {
    char name[64];
    int id;
} s_type_cache[MAX_CACHE];
static int s_cache_count = 0;

semantic_type sdk_type_lookup(plugin_ctx* ctx, const char* name) {
    // Check cache
    for (int i = 0; i < s_cache_count; i++) {
        if (strcmp(s_type_cache[i].name, name) == 0) {
            return s_type_cache[i].id;
        }
    }

    // Query Core
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"lookup\", \"name\":\"%s\"}", name);
    if (sdk_ipc_send(ctx, buf) < 0) return -1;

    char resp[256];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0) return -1;

    // Parse response "{\"id\":123}"
    // Hardcoded parse for MVP to avoid json dep here? Or use sdk_json?
    // Let's rely on sscanf for simple response
    int id = -1;
    // Assume response is just the ID as a string or simple JSON?
    // Let's assume Core returns simple JSON: {"id":123}
    // Quick, dirty parse
    char* p = strstr(resp, "\"id\":");
    if (p) {
        id = atoi(p + 5);
    } else {
        return -1;
    }

    // Cache it
    if (s_cache_count < MAX_CACHE) {
        strncpy(s_type_cache[s_cache_count].name, name, 63);
        s_type_cache[s_cache_count].id = id;
        s_cache_count++;
    }

    return id;
}

void sdk_log(const plugin_ctx* ctx, sdk_log_level level, const char* fmt, ...) {
    if (!ctx) return;
    char payload[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(payload, sizeof(payload), fmt, args);
    va_end(args);

    char buf[1024];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"log\", \"lvl\":%d, \"msg\":\"%s\"}", level, payload);
    sdk_ipc_send((plugin_ctx*)ctx, buf);  // const cast
}
