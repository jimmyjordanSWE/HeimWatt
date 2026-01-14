#ifndef PIPELINE_H
#define PIPELINE_H

#include "types.h"

/**
 * @brief Main pipeline entry point.
 * Runs the data fetching and planning loop in a separate thread.
 * @param arg Pointer to server_ctx.
 * @return NULL (always).
 */
void* pipeline_thread_func(void* arg);

/**
 * @brief Initial data backfill helper.
 * Populates database with historical data if empty.
 * @param ctx Server context.
 */
void pipeline_backfill(server_ctx* ctx);

#endif  // PIPELINE_H
