/*
 * @file signal_util.h
 * @brief Signal handling utilities
 *
 * Provides graceful shutdown via signal handling.
 */

#ifndef HEIMWATT_SIGNAL_UTIL_H
#define HEIMWATT_SIGNAL_UTIL_H

/*
 * Setup signal handlers for graceful shutdown.
 * Handles SIGINT, SIGTERM.
 *
 * @return 0 on success, -1 on error
 */
int signal_setup(void);

/*
 * Check if shutdown has been requested.
 *
 * @return 1 if shutdown requested, 0 otherwise
 */
int signal_shutdown_requested(void);

/*
 * Request shutdown programmatically.
 * Thread-safe.
 */
void signal_request_shutdown(void);

/*
 * Block signals in current thread.
 * Useful for worker threads.
 *
 * @return 0 on success, -1 on error
 */
int signal_block_all(void);

#endif /* HEIMWATT_SIGNAL_UTIL_H */
