#include "memory.h"
#include "sdk_internal.h"

#include <heimwatt_sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sdk_query_latest(plugin_ctx* ctx, semantic_type type, sdk_data_point* out) {
    if (!ctx || !out)
        return -1;

    char buf[128];
    (void) snprintf(buf, sizeof(buf), "{\"cmd\":\"query_latest\", \"sem\":%d}", type);

    if (sdk_ipc_send(ctx, buf) < 0)
        return -1;

    // Receive response (expecting single line JSON)
    // Using a smaller buffer as response is small: {"val":..., "ts":...}
    char resp_buf[256];
    if (sdk_ipc_recv(ctx, resp_buf, sizeof(resp_buf)) < 0)
        return -1;

    json_value* root = sdk_json_parse(resp_buf);
    if (!root)
        return -1;

    const json_value* val_node = sdk_json_get(root, "val");
    const json_value* ts_node = sdk_json_get(root, "ts");

    if (!val_node || !ts_node) {
        // Not found or empty response
        sdk_json_free(root);
        return -1;
    }

    out->value = sdk_json_number(val_node);
    out->timestamp = (int64_t) sdk_json_number(ts_node);
    memset(out->currency, 0, 4);  // Stub

    sdk_json_free(root);
    return 0;
}

int sdk_query_history(plugin_ctx* ctx, semantic_type type, int64_t from_ts, int64_t to_ts,
                      sdk_data_point** out_array, size_t* out_count) {
    if (!ctx || !out_array || !out_count)
        return -1;

    char buf[256];
    (void) snprintf(buf, sizeof(buf),
                    "{\"cmd\":\"query_range\", \"sem\":%d, \"from\":%ld, \"to\":%ld}", type,
                    from_ts, to_ts);

    if (sdk_ipc_send(ctx, buf) < 0)
        return -1;

    // Receive potentially large response?
    // sdk_ipc_recv (MVP) only reads 4KB. Only empty responses work for now.
    // If response > 4KB, this breaks. Alpha Limitation.
    char* resp_buf = mem_alloc(4096);
    if (!resp_buf)
        return -1;

    if (sdk_ipc_recv(ctx, resp_buf, 4096) < 0) {
        mem_free(resp_buf);
        return -1;
    }

    // Parse: {"count": 0, ...}
    // Using sdk_json helper would require implementing a parser?
    // sdk_json_parse uses cJSON.
    json_value* root = sdk_json_parse(resp_buf);
    mem_free(resp_buf);
    if (!root)
        return -1;

    size_t count = (size_t) sdk_json_int(sdk_json_get(root, "count"));
    if (count == 0) {
        *out_array = NULL;
        *out_count = 0;
        sdk_json_free(root);
        return 0;
    }

    const json_value* vals = sdk_json_get(root, "values");
    const json_value* tss = sdk_json_get(root, "ts");

    if (!vals || !tss) {
        sdk_json_free(root);
        return -1;
    }

    sdk_data_point* points = mem_alloc(count * sizeof(*points));
    if (!points) {
        sdk_json_free(root);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        points[i].value = sdk_json_number(sdk_json_array_get(vals, i));
        points[i].timestamp = (int64_t) sdk_json_number(sdk_json_array_get(tss, i));
        // Currency is optional/stubbed for now
        memset(points[i].currency, 0, 4);
    }

    *out_array = points;
    *out_count = count;

    sdk_json_free(root);
    return 0;
}

void sdk_data_point_destroy(sdk_data_point** points_ptr) {
    if (points_ptr && *points_ptr) {
        mem_free(*points_ptr);
        *points_ptr = NULL;
    }
}
