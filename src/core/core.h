/**
 * @file core.h
 * @brief System lifecycle and orchestration
 *
 * The Core module is the central nervous system of HeimWatt.
 * It ties together all other modules and manages the system lifecycle.
 */

#ifndef HEIMWATT_CORE_H
#define HEIMWATT_CORE_H

#include <stdbool.h>

typedef struct heimwatt_ctx heimwatt_ctx;

/**
 * Initialize the HeimWatt core system.
 * Loads config, opens database, starts IPC server, scans plugins.
 *
 * @param ctx         Output pointer for context (set to NULL on failure)
 * @param config_path Path to configuration file
 * @return 0 on success, -1 on error
 */
int core_init(heimwatt_ctx** ctx, const char* config_path);

/**
 * Run the main event loop.
 * Starts HTTP server, starts plugins, blocks until shutdown.
 *
 * @param ctx HeimWatt context
 * @return 0 on normal shutdown, -1 on error
 */
int core_run(heimwatt_ctx* ctx);

/**
 * Request graceful shutdown.
 * Signals all plugins to stop, closes connections.
 *
 * @param ctx HeimWatt context
 */
void core_shutdown(heimwatt_ctx* ctx);

/**
 * Destroy context and free all resources.
 *
 * @param ctx Pointer to context (set to NULL on return)
 */
void core_destroy(heimwatt_ctx** ctx);

/**
 * Check if system is running.
 *
 * @param ctx HeimWatt context
 * @return true if running, false otherwise
 */
bool core_is_running(const heimwatt_ctx* ctx);

#endif /* HEIMWATT_CORE_H */
