#include "sdk_internal.h"

#include <heimwatt_sdk.h>
#include <net/http_client.h>
#include <net/json.h>
#include <stddef.h>
#include <stdlib.h>

int sdk_http_get(plugin_ctx* ctx, const char* url, char** body_out) {
    if (!ctx || !url || !body_out)
        return -1;

    // Lazy init client
    if (!ctx->http_client) {
        if (http_client_create((http_client**) &ctx->http_client) < 0)
            return -1;
    }

    size_t len = 0;
    // Assume http_get expects http_client*
    return http_get((http_client*) ctx->http_client, url, body_out, &len);
}

int sdk_fetch_json(plugin_ctx* ctx, const char* url, json_value** out) {
    if (!ctx || !url || !out)
        return -1;

    char* body = NULL;
    int status = sdk_http_get(ctx, url, &body);

    if (status < 200 || status >= 300) {
        if (body)
            free(body);
        return -1;
    }

    *out = sdk_json_parse(body);
    free(body);

    return (*out) ? 0 : -1;
}
