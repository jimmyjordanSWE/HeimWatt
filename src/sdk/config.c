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
            *value = malloc(len + 1);
            strncpy(*value, p, len);
            (*value)[len] = 0;
            return 0;
        }
    }

    return -1;  // Not found
}
