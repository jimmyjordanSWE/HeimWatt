/**
 * @file sdk_core.h
 * @brief SDK context lifecycle (internal)
 *
 * Internal context structure and lifecycle functions.
 */

#ifndef SDK_CORE_H
#define SDK_CORE_H

#include <stddef.h>

#include "sdk_ipc.h"
#include "heimwatt_sdk.h"

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
        sdk_api_handler_t handler;
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
 * Initialize plugin context.
 *
 * @param ctx  Output pointer for context
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @return 0 on success, -1 on error
 */
int sdk_core_init(plugin_ctx** ctx, int argc, char** argv);

/**
 * Finalize plugin context.
 *
 * @param ctx Pointer to context (set to NULL on return)
 */
void sdk_core_fini(plugin_ctx** ctx);

/**
 * Run plugin event loop.
 *
 * @param ctx Plugin context
 * @return 0 on normal shutdown, -1 on error
 */
int sdk_core_run(plugin_ctx* ctx);

#endif /* SDK_CORE_H */
