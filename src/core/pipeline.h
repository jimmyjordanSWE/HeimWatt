/*
 * @file pipeline.h
 * @brief Internal data pipeline
 *
 * Runs the data fetching and planning loop in a separate thread.
 * This is an internal module, not part of the public API.
 */

#ifndef HEIMWATT_PIPELINE_H
#define HEIMWATT_PIPELINE_H

/* Forward declaration */
typedef struct heimwatt_ctx heimwatt_ctx;

/*
 * Main pipeline entry point.
 * Runs the data fetching and planning loop in a separate thread.
 *
 * @param arg Pointer to heimwatt_ctx.
 * @return NULL (always).
 *
 * @note Internal use only.
 */
void* pipeline_thread_func(void* arg);

/*
 * Initial data backfill helper.
 * Populates database with historical data if empty.
 *
 * @param ctx HeimWatt context.
 *
 * @note Internal use only.
 */
void pipeline_backfill(heimwatt_ctx* ctx);

#endif /* HEIMWATT_PIPELINE_H */
