#ifndef HEIMWATT_SERVER_H
#define HEIMWATT_SERVER_H

/**
 * @file server.h
 * @brief HeimWatt Core Lifecycle API
 *
 * Public API for controlling the HeimWatt core system.
 * This is the main entry point for applications embedding HeimWatt.
 *
 * ## Thread Safety
 *
 * - `heimwatt_create()`: NOT thread-safe, call once from main thread
 * - `heimwatt_init()`: NOT thread-safe
 * - `heimwatt_run()`: Blocks, handles signals internally
 * - `heimwatt_request_shutdown()`: Thread-safe, signal-safe
 * - `heimwatt_destroy()`: NOT thread-safe, call after run returns
 * - `heimwatt_is_running()`: Thread-safe, atomic read
 *
 * ## Typical Usage
 *
 * ```c
 * heimwatt_ctx* ctx = heimwatt_create();
 * if (heimwatt_init(ctx, "config.json") != 0) {
 *     heimwatt_destroy(&ctx);
 *     return 1;
 * }
 * heimwatt_run(ctx);  // Blocks until shutdown
 * heimwatt_destroy(&ctx);
 * ```
 */

#include <signal.h>
#include <stdbool.h>

/**
 * @brief Opaque HeimWatt context.
 *
 * Represents a running HeimWatt instance.
 * Created with heimwatt_create(), destroyed with heimwatt_destroy().
 */
typedef struct heimwatt_ctx heimwatt_ctx;

/**
 * @brief Create a new HeimWatt context.
 *
 * Allocates but does not initialize the context.
 * Must call heimwatt_init() before heimwatt_run().
 *
 * @return Pointer to context on success, NULL on allocation failure.
 *
 * @threadsafe No.
 */
heimwatt_ctx *heimwatt_create(void);

/**
 * @brief Initialize the system.
 *
 * Loads config, connects to DB, starts IPC server.
 *
 * @param ctx Context to initialize.
 * @param base_path Storage base path (e.g., "data/stockholm").
 * @return 0 on success, -1 on failure.
 *
 * @threadsafe No.
 */
int heimwatt_init(heimwatt_ctx *ctx, const char *base_path);

/**
 * @brief Run the main loop.
 *
 * Blocks until shutdown is requested (signal or heimwatt_request_shutdown()).
 * Starts plugins, HTTP server, IPC handler.
 *
 * @param ctx Initialized context.
 *
 * @threadsafe No (caller must be main thread).
 */
void heimwatt_run(heimwatt_ctx *ctx);

/**
 * @brief Run the main loop with external shutdown flag.
 *
 * Like heimwatt_run(), but polls an external shutdown flag.
 * This allows signal handlers to trigger shutdown in an async-signal-safe
 * manner by setting the flag without calling non-signal-safe functions.
 *
 * @param ctx Initialized context.
 * @param shutdown_flag Pointer to volatile flag (set to non-zero to shutdown).
 *
 * @threadsafe No (caller must be main thread).
 */
void heimwatt_run_with_shutdown_flag(heimwatt_ctx *ctx, const volatile sig_atomic_t *shutdown_flag);

/**
 * @brief Request graceful shutdown.
 *
 * Signals heimwatt_run() to exit gracefully.
 * Safe to call from any thread or signal handler.
 *
 * @param ctx Context to shut down.
 *
 * @threadsafe Yes.
 * @signalsafe Yes.
 */
void heimwatt_request_shutdown(heimwatt_ctx *ctx);

/**
 * @brief Destroy context and free resources.
 *
 * Call after heimwatt_run() returns.
 *
 * @param ctx_ptr Pointer to context pointer (set to NULL on return).
 *
 * @threadsafe No.
 */
void heimwatt_destroy(heimwatt_ctx **ctx_ptr);

/**
 * @brief Check if system is running.
 *
 * @param ctx Context to check.
 * @return true if running, false otherwise.
 *
 * @threadsafe Yes.
 */
bool heimwatt_is_running(const heimwatt_ctx *ctx);

#endif /* HEIMWATT_SERVER_H */
