#ifndef SERVER_H
#define SERVER_H

#include "db.h"
#include "types.h"

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Create a new server context.
 * Allocates and zero-initializes the context structure.
 * @return Pointer to context on success, NULL on allocation failure.
 */
server_ctx* server_create(void);

/**
 * @brief Destroy server context and free memory.
 * Calls server_fini() if needed, then frees the context.
 * @param ctx_ptr Pointer to context pointer (set to NULL after free).
 */
void server_destroy(server_ctx** ctx_ptr);

/**
 * @brief Initialize the server context.
 * Sets up sockets, database connection, and synchronization primitives.
 * @param ctx Context to initialize.
 * @return 0 on success, -1 on failure.
 */
int server_init(server_ctx* ctx);

/**
 * @brief Run the main server loop.
 * Blocks until shutdown is requested.
 * @param ctx Initialized server context.
 */
void server_run(server_ctx* ctx);

/**
 * @brief Finalize and cleanup server resources.
 * Closes sockets, DB, and destroys mutexes.
 * @param ctx Context to cleanup.
 */
void server_fini(server_ctx* ctx);

/**
 * @brief Update the shared weather state.
 * Thread-safe.
 * @param ctx Server context.
 * @param w New weather data.
 */
void server_update_weather(server_ctx* ctx, const weather_data* w);

/**
 * @brief Update the shared price state and forecast.
 * Thread-safe.
 * @param ctx Server context.
 * @param p Current spot price.
 * @param forecast_24h Optional 24h price forecast array (can be NULL).
 */
void server_update_price(server_ctx* ctx, const spot_price* p, const float* forecast_24h);

/**
 * @brief Update the shared energy plan.
 * Thread-safe.
 * @param ctx Server context.
 * @param plan New energy plan.
 */
void server_update_plan(server_ctx* ctx, const energy_plan* plan);

/**
 * @brief Set the next scheduled fetch times.
 * Thread-safe.
 * @param ctx Server context.
 * @param next_weather Timestamp for next weather fetch.
 * @param next_price Timestamp for next price fetch.
 */
void server_set_next_fetch(server_ctx* ctx, time_t next_weather, time_t next_price);

// Accessors (for pipeline)

/**
 * @brief Check if server is running.
 * @param ctx Server context.
 * @return true if running, false if shutdown requested.
 */
bool server_is_running(const server_ctx* ctx);

/**
 * @return Pointer to database handle.
 */
db_handle* server_get_db(server_ctx* ctx);

/**
 * @brief Get the configuration object.
 * @param ctx Server context.
 * @return Pointer to read-only config.
 */
const config* server_get_config(server_ctx* ctx);

/**
 * @brief Get mutable configuration object.
 * Used during initialization to set config values.
 * @param ctx Server context.
 * @return Pointer to mutable config.
 */
config* server_get_config_mutable(server_ctx* ctx);

/**
 * @brief Get pipeline thread handle.
 * Used by main to create/join pipeline thread.
 * @param ctx Server context.
 * @return Pointer to pthread_t.
 */
pthread_t* server_get_pipeline_thread(server_ctx* ctx);

/**
 * @brief Set server running state.
 * @param ctx Server context.
 * @param running New running state.
 */
void server_set_running(server_ctx* ctx, bool running);

#endif  // SERVER_H
