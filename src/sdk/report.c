#include <heimwatt_sdk.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/cJSON.h"
#include "sdk_internal.h"

int sdk_report(plugin_ctx *ctx, const sdk_metric *metric)
{
    if (!ctx || !metric) return -1;

    char buf[1024];
    // Format: {"cmd":"report", "semantic":ID, "val":123.4, "ts":123456789, "curr":"SEK"}

    char currency_part[64] = "";
    if (metric->currency)
    {
        snprintf(currency_part, sizeof(currency_part), ", \"curr\":\"%s\"", metric->currency);
    }

    snprintf(buf, sizeof(buf), "{\"cmd\":\"report\", \"sem\":%d, \"val\":%f, \"ts\":%ld%s}",
             metric->semantic, metric->value, metric->timestamp, currency_part);

    return sdk_ipc_send(ctx, buf);
}

int sdk_report_value(plugin_ctx *ctx, const char *type_name, double value, int64_t ts)
{
    if (!ctx || !type_name) return -1;

    // MVP: Send string type directly (server expects "type", "val", "ts")
    char buf[1024];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"report\", \"type\":\"%s\", \"val\":%f, \"ts\":%ld}",
             type_name, value, ts);
    return sdk_ipc_send(ctx, buf);
}

int sdk_report_price(plugin_ctx *ctx, const char *type_name, double value, const char *currency,
                     int64_t ts)
{
    // MVP: Send string type directly
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "{\"cmd\":\"report\", \"type\":\"%s\", \"val\":%f, \"ts\":%ld, \"curr\":\"%s\"}",
             type_name, value, ts, currency ? currency : "");
    return sdk_ipc_send(ctx, buf);
}

// ... lookup cache ignored ...

semantic_type sdk_type_lookup(plugin_ctx *ctx, const char *name)
{
    (void) ctx;
    return semantic_from_string(name);
}

void sdk_log(const plugin_ctx *ctx, sdk_log_level level, const char *fmt, ...)
{
    if (!ctx) return;

    char payload[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(payload, sizeof(payload), fmt, args);
    va_end(args);

    // Send to IPC
    // Use cJSON or manual format. cJSON is already in SDK.
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "cmd", "log");
    cJSON_AddNumberToObject(json, "level", (double) level);
    cJSON_AddStringToObject(json, "msg", payload);

    char *str = cJSON_PrintUnformatted(json);
    if (str)
    {
        // Cast away const - sdk_ipc_send only needs fd
        sdk_ipc_send((plugin_ctx *) ctx, str);
        free(str);
    }
    cJSON_Delete(json);
}
