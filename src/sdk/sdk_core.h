/**
 * @file sdk_core.h
 * @brief SDK context lifecycle (internal)
 *
 * Internal context structure and lifecycle functions.
 */

#ifndef HEIMWATT_SDK_CORE_H
#define HEIMWATT_SDK_CORE_H

#include <errno.h>
#include <stddef.h>

#include "heimwatt_sdk.h"
#include "sdk_ipc.h"

/**
 * Internal plugin context structure.
 */
struct plugin_ctx {
    char* plugin_id;
    ipc_client* ipc;

    /* Registered endpoints */
    struct {
        char* method;
        char* path;
        sdk_api_handler handler;
    } endpoints[32];
    size_t endpoint_count;

    /* Required semantics */
    semantic_type required[64];
    size_t required_count;

    /* Optional semantics */
    semantic_type optional[64];
    size_t optional_count;

    /* State */
    volatile int running;
};

/* ============================================================
 * INTERNAL FUNCTIONS
 * ============================================================ */

/**
 * Create and initialize plugin context.
 *
 * @param ctx  Output pointer for context
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @return 0 on success, negative errno on error
 */
int sdk_core_create(plugin_ctx** ctx, int argc, char** argv);

/**
 * Destroy plugin context.
 *
 * @param ctx Pointer to context (set to NULL on return)
 */
void sdk_core_destroy(plugin_ctx** ctx);

/**
 * Run plugin event loop.
 *
 * @param ctx Plugin context
 * @return 0 on normal shutdown, negative errno on error
 */
int sdk_core_run(plugin_ctx* ctx);

#endif  // HEIMWATT_SDK_CORE_H */
