#include <heimwatt_sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdk_internal.h"

// Stubs for Calculator Plugins (OUT)
// In a real implementation, these would handle IPC for endpoint registration
// and response buffering. For Alpha, we provide stubs to allow linking.

const char *sdk_req_method(const sdk_req *req)
{
    (void) req;
    return "GET";
}

const char *sdk_req_path(const sdk_req *req)
{
    (void) req;
    return "/";
}

const char *sdk_req_query_param(const sdk_req *req, const char *key)
{
    if (!req || !key) return NULL;

    // Simple parsing of "key=value&key2=val2"
    // Note: This returns a pointer into the query string (dangerous if not careful?)
    // Actually, we should probably return a copy or token.
    // For MVP, we'll scan and return a static buffer or modify in place?
    // Const correctness issue.
    // Let's copy to a static thread-local buffer for simplicity in MVP.
    static char val_buf[256];

    char *q = strdup(req->query);
    if (!q) return NULL;

    char *token = strtok(q, "&");
    while (token)
    {
        // key=value
        char *eq = strchr(token, '=');
        if (eq)
        {
            *eq = 0;
            if (strcmp(token, key) == 0)
            {
                snprintf(val_buf, sizeof(val_buf), "%s", eq + 1);
                free(q);
                return val_buf;
            }
        }
        token = strtok(NULL, "&");
    }

    free(q);
    return NULL;
}

int sdk_register_endpoint(plugin_ctx *ctx, const char *method, const char *path,
                          sdk_api_handler handler)
{
    if (!ctx || !method || !path || !handler) return -1;
    if (ctx->endpoint_count >= SDK_MAX_ENDPOINTS) return -1;

    // Store in context
    sdk_endpoint_entry *e = &ctx->endpoints[ctx->endpoint_count++];
    snprintf(e->method, sizeof(e->method), "%s", method);
    snprintf(e->path, sizeof(e->path), "%s", path);
    e->handler = handler;

    // Send registration command to Core via IPC
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"cmd\":\"register_endpoint\", \"method\":\"%s\", \"path\":\"%s\"}", method, path);

    sdk_log(ctx, SDK_LOG_INFO, "Registering endpoint: %s %s", method, path);
    return sdk_ipc_send(ctx, buf);
}

void sdk_resp_set_status(sdk_resp *resp, int code)
{
    if (resp) resp->status = code;
}

void sdk_resp_set_json(sdk_resp *resp, const char *json_body)
{
    if (resp && json_body)
    {
        snprintf(resp->body, sizeof(resp->body), "%s", json_body);
        // Ensure null termination although snprintf does it
    }
}

void sdk_resp_set_header(sdk_resp *resp, const char *key, const char *val)
{
    (void) resp;
    (void) key;
    (void) val;
}

int sdk_require_semantic(plugin_ctx *ctx, semantic_type type)
{
    if (!ctx) return -1;
    (void) type;
    return 0;
}
