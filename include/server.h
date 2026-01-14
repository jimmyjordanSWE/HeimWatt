#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Opaque HeimWatt context.
 * Defines the application instance.
 */
typedef struct heimwatt_ctx heimwatt_ctx;

/**
 * @brief Create a new HeimWatt context.
 * @return Pointer to context on success, NULL on failure.
 */
heimwatt_ctx* heimwatt_create(void);

/**
 * @brief Initialize the system.
 * Loads config, connects to DB, starts IPC.
 * @param ctx Context to initialize.
 * @param config_path Path to configuration JSON (optional).
 * @return 0 on success, -1 on failure.
 */
int heimwatt_init(heimwatt_ctx* ctx, const char* config_path);

/**
 * @brief Run the main loop.
 * Blocks until shutdown.
 * Starts plugins, HTTP server, etc.
 * @param ctx Initialized context.
 */
void heimwatt_run(heimwatt_ctx* ctx);

/**
 * @brief Request shutdown.
 * Unblocks heimwatt_run().
 */
void heimwatt_request_shutdown(heimwatt_ctx* ctx);

/**
 * @brief Destroy context.
 * @param ctx_ptr Pointer to context pointer.
 */
void heimwatt_destroy(heimwatt_ctx** ctx_ptr);

/**
 * @brief Check if system is running.
 */
bool heimwatt_is_running(const heimwatt_ctx* ctx);

#endif  // SERVER_H
