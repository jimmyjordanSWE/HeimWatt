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

int sdk_credential_get(plugin_ctx *ctx, const char *key, char **value)
{
    if (!ctx || !key || !value) return -1;

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"credential_get\", \"key\":\"%s\"}", key);

    if (sdk_ipc_send(ctx, buf) < 0) return -1;

    char resp[1024];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0) return -1;

    // {"value":"..."}
    char *p = strstr(resp, "\"value\":\"");
    if (p)
    {
        p += 9;
        char *end = strchr(p, '"');
        if (end)
        {
            size_t len = end - p;
            char *raw_val = mem_alloc(len + 1);
            if (!raw_val) return -1;
            memcpy(raw_val, p, len);
            raw_val[len] = '\0';
            *value = raw_val;
            return 0;
        }
    }
    return -1;
}

void sdk_credential_destroy(char **value)
{
    if (value && *value)
    {
        // Zero out memory for security because it's a credential
        memset(*value, 0, strlen(*value));
        mem_free(*value);
        *value = NULL;
    }
}

int sdk_device_setpoint(plugin_ctx *ctx, const char *device_id, double value)
{
    if (!ctx || !device_id) return -1;

    char buf[512];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"device_setpoint\", \"id\":\"%s\", \"value\":%.6f}",
             device_id, value);

    if (sdk_ipc_send(ctx, buf) < 0) return -1;

    // Wait for ACK
    char resp[256];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0) return -1;

    // Check for "status":"ok"
    if (strstr(resp, "\"status\":\"ok\"")) return 0;

    // Check for error
    if (strstr(resp, "\"error\""))
    {
        // Log error is hard here since we don't parse it fully, but return -1
        return -1;
    }

    return 0;
}
