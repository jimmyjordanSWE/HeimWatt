#ifndef SDK_EVENTLOOP_H
#define SDK_EVENTLOOP_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Opaque handle to an SDK event loop.
 */
typedef struct sdk_eventloop sdk_eventloop;

/**
 * @brief Callback for file descriptor events.
 * @param ctx   User context pointer.
 * @param fd    The file descriptor that triggered the event.
 * @param event The event flags (e.g., POLLIN).
 */
typedef void (*sdk_fd_callback)(void *ctx, int fd, int event);

/**
 * @brief Callback for time-based ticker events.
 * @param ctx User context pointer.
 * @param now Current timestamp (seconds since epoch).
 */
typedef void (*sdk_ticker_callback)(void *ctx, int64_t now);

/**
 * @brief Creates a new event loop instance.
 * @return Pointer to new event loop, or NULL on failure.
 */
sdk_eventloop *sdk_eventloop_create(void);

/**
 * @brief Destroys an event loop instance and frees resources.
 * @param loop_ptr Pointer to the loop pointer variable. Will be set to NULL.
 */
void sdk_eventloop_destroy(sdk_eventloop **loop_ptr);

/**
 * @brief Registers a file descriptor to be monitored.
 * @param loop   The event loop instance.
 * @param fd     The file descriptor to monitor.
 * @param events Poll events to monitor (e.g., POLLIN).
 * @param cb     Callback function to invoke when event occurs.
 * @param ctx    Context pointer passed to the callback.
 * @return 0 on success, negative errno on failure.
 */
int sdk_eventloop_add_fd(sdk_eventloop *loop, int fd, int events, sdk_fd_callback cb, void *ctx);

/**
 * @brief Removes a file descriptor from monitoring.
 * @param loop The event loop instance.
 * @param fd   The file descriptor to remove.
 * @return 0 on success, negative errno on failure.
 */
int sdk_eventloop_remove_fd(sdk_eventloop *loop, int fd);

/**
 * @brief Registers a periodic ticker callback.
 * @param loop         The event loop instance.
 * @param interval_sec Calling interval in seconds.
 * @param cb           Callback function to invoke.
 * @param ctx          Context pointer passed to the callback.
 * @return 0 on success, negative errno on failure.
 */
int sdk_eventloop_add_ticker(sdk_eventloop *loop, int interval_sec, sdk_ticker_callback cb,
                             void *ctx);

/**
 * @brief internal Cron support structure (opaque)
 * could be expanded later if we fully move logic here,
 * for now we just support basic tickers.
 * But sdk/lifecycle.c supports cron strings.
 * We might want to support custom next_run function or similar?
 * For MVP of this refactor, we can simple use a "generic" ticker
 * that calculates its own next run time?
 * Or we can overload add_ticker or add a more generic one.
 */

/*
 * Let's keep it simple for now matching the plan:
 * Ticker just does interval.
 * Actually lifecycle logic has Cron.
 * If we want to move logic here, we need to support it.
 * But maybe we can abstract it: "Next Run Time" logic.
 *
 * For now, let's stick to the interface in the plan:
 * sdk_eventloop_add_ticker(..., interval, ...)
 *
 * If the user needs cron, they can implement it on top or we add
 * sdk_eventloop_add_cron(...) later.
 * Wait, lifecycle.c ALREADY has cron support.
 * We should support it to avoid regressions.
 */

// Adding Cron support api
typedef int64_t (*sdk_next_run_fn)(void *ctx, int64_t now);

/**
 * @brief Registers a custom scheduled task.
 * @param loop         The event loop instance.
 * @param next_run_fn  Function to calculate the next run time.
 * @param cb           Callback function to execute.
 * @param ctx          Context pointer.
 * @return 0 on success, negative error code.
 */
int sdk_eventloop_add_scheduled_task(sdk_eventloop *loop, sdk_next_run_fn next_run_fn,
                                     sdk_ticker_callback cb, void *ctx);

/**
 * @brief Runs the event loop. This function blocks until sdk_eventloop_stop is called.
 * @param loop The event loop instance.
 * @return 0 on success, negative errno on failure.
 */
int sdk_eventloop_run(sdk_eventloop *loop);

/**
 * @brief Signals the event loop to stop running.
 * Safe to call from callbacks.
 * @param loop The event loop instance.
 */
void sdk_eventloop_stop(sdk_eventloop *loop);

#endif  // SDK_EVENTLOOP_H
