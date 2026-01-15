#include <heimwatt_sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdk_internal.h"

int sdk_get_config(plugin_ctx* ctx, const char* key, char** value) {
    if (!ctx || !key || !value) return -1;

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"config\", \"key\":\"%s\"}", key);

    if (sdk_ipc_send(ctx, buf) < 0) return -1;

    // Expect response: {"val":"..."} or {}
    char resp[1024];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0) return -1;

    // Quick dirty parse
    char* p = strstr(resp, "\"val\":\"");
    if (p) {
        p += 7;
        char* end = strchr(p, '"');
        if (end) {
            size_t len = end - p;
            char* raw_val = malloc(len + 1);
            strncpy(raw_val, p, len);
            raw_val[len] = 0;

            // Variable Substitution
            if (ctx->in_backfill_mode && ctx->current_tick_time > 0) {
                // Check for placeholders
                if (strstr(raw_val, "{date}") || strstr(raw_val, "{iso}")) {
                    struct tm tm;
                    gmtime_r((time_t*)&ctx->current_tick_time, &tm);

                    char date_str[32];  // YYYY-MM-DD
                    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);

                    char iso_str[32];  // YYYY-MM-DDTHH:MM:SSZ
                    strftime(iso_str, sizeof(iso_str), "%Y-%m-%dT%H:%M:%SZ", &tm);

                    // Reconstruct string
                    // For MVP: Simple single replacement or naive implementation
                    // 1. Calculate new length
                    // Assumes simple replacement for now.

                    // Actually, let's just implement a simple replace helper or inline it if
                    // simple. Given C string pain, let's assume max 1 occurrence or handle simpler
                    // case. To be robust:

                    char* new_val = malloc(4096);  // Generous buffer
                    char* src = raw_val;
                    char* dst = new_val;
                    char* dst_end = new_val + 4096 - 1;

                    while (*src && dst < dst_end) {
                        if (strncmp(src, "{date}", 6) == 0) {
                            int l = strlen(date_str);
                            if (dst + l < dst_end) {
                                strcpy(dst, date_str);
                                dst += l;
                                src += 6;
                            } else
                                break;
                        } else if (strncmp(src, "{iso}", 5) == 0) {
                            int l = strlen(iso_str);
                            if (dst + l < dst_end) {
                                strcpy(dst, iso_str);
                                dst += l;
                                src += 5;
                            } else
                                break;
                        } else {
                            *dst++ = *src++;
                        }
                    }
                    *dst = 0;
                    free(raw_val);
                    *value = new_val;
                    return 0;
                }
            }

            *value = raw_val;
            return 0;
        }
    }

    return -1;  // Not found
}
