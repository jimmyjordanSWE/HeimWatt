#include <heimwatt_sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "sdk_internal.h"

int sdk_get_config(plugin_ctx *ctx, const char *key, char **value)
{
    if (!ctx || !key || !value) return -1;

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"config\", \"key\":\"%s\"}", key);

    if (sdk_ipc_send(ctx, buf) < 0) return -1;

    // Expect response: {"val":"..."} or {}
    char resp[1024];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0) return -1;

    // Quick dirty parse
    char *p = strstr(resp, "\"val\":\"");
    if (p)
    {
        p += 7;
        char *end = strchr(p, '"');
        if (end)
        {
            size_t len = end - p;
            char *raw_val = mem_alloc(len + 1);
            if (!raw_val) return -1;
            memcpy(raw_val, p, len);
            raw_val[len] = '\0';

            // Handle variable substitution
            if (ctx->in_backfill_mode && ctx->current_tick_time > 0)
            {
                char *processed = NULL;
                int ret = sdk_substitute_config_vars(raw_val, (time_t) ctx->current_tick_time,
                                                     &processed);
                if (ret == 0 && processed)
                {
                    mem_free(raw_val);
                    *value = processed;
                    return 0;
                }
            }

            *value = raw_val;
            return 0;
        }
    }

    return -1;  // Not found
}

int sdk_substitute_config_vars(const char *input, time_t now, char **output)
{
    if (!input || !output) return -1;

    // Check for placeholders
    if (!strstr(input, "{date}") && !strstr(input, "{iso}"))
    {
        size_t slen = strlen(input);
        *output = mem_alloc(slen + 1);
        if (*output) strcpy(*output, input);
        return 0;
    }

    struct tm tm;
    gmtime_r(&now, &tm);

    char date_str[32];  // YYYY-MM-DD
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);

    char iso_str[32];  // YYYY-MM-DDTHH:MM:SSZ
    strftime(iso_str, sizeof(iso_str), "%Y-%m-%dT%H:%M:%SZ", &tm);

    char *new_val = mem_alloc(4096);
    if (!new_val) return -1;

    const char *src = input;
    char *dst = new_val;
    char *dst_end = new_val + 4096 - 1;

    while (*src && dst < dst_end)
    {
        if (strncmp(src, "{date}", 6) == 0)
        {
            size_t l = strlen(date_str);
            if (dst + l < dst_end)
            {
                memcpy(dst, date_str, l);
                dst += l;
                src += 6;
            }
            else
            {
                // Truncate or error? For now break to avoid overflow
                break;
            }
        }
        else if (strncmp(src, "{iso}", 5) == 0)
        {
            size_t l = strlen(iso_str);
            if (dst + l < dst_end)
            {
                memcpy(dst, iso_str, l);
                dst += l;
                src += 5;
            }
            else
            {
                break;
            }
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = 0;

    *output = new_val;
    return 0;
}
