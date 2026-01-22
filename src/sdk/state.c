#include "memory.h"
#include "sdk_internal.h"

#include <heimwatt_sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sdk_state_save(plugin_ctx* ctx, const char* key, const char* value) {
    if (!ctx || !key || !value)
        return -1;

    char buf[1024];
    // TODO: better escaping for key/value in JSON?
    // For MVP assuming simple strings.
    snprintf(buf, sizeof(buf), "{\"cmd\":\"state_save\", \"key\":\"%s\", \"val\":\"%s\"}", key,
             value);

    // Fire and forget? Or wait for ack?
    // Usually save should confirm.
    if (sdk_ipc_send(ctx, buf) < 0)
        return -1;

    char resp[64];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0)
        return -1;

    // Check for "ok" or error?
    // Core returns {"status":"ok"} or {"status":"error"}
    if (strstr(resp, "\"ok\""))
        return 0;
    return -1;
}

int sdk_state_load(plugin_ctx* ctx, const char* key, char** value_out) {
    if (!ctx || !key || !value_out)
        return -1;

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"state_load\", \"key\":\"%s\"}", key);

    if (sdk_ipc_send(ctx, buf) < 0)
        return -1;

    char resp[1024];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0)
        return -1;

    char* p = strstr(resp, "\"val\":\"");
    if (p) {
        p += 7;
        char* end = strchr(p, '"');
        if (end) {
            size_t len = end - p;
            *value_out = mem_alloc(len + 1);
            if (!*value_out)
                return -1;
            memcpy(*value_out, p, len);
            (*value_out)[len] = 0;
            return 0;
        }
    }

    return -1;
}
