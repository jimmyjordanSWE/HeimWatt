#ifndef HEIMWATT_THREAD_POOL_H
#define HEIMWATT_THREAD_POOL_H

#include <stddef.h>

typedef struct thread_pool thread_pool;

/**
 * @brief Create a new thread pool.
 *
 * @param worker_count Number of worker threads to spawn.
 * @return Pointer to new pool, or NULL on failure.
 */
thread_pool *thread_pool_create(int worker_count);

/**
 * @brief Destroy the thread pool.
 *        Stops all workers and frees memory.
 *        Pending tasks may not be executed if stop is immediate.
 *
 * @param pool_ptr Pointer to the pool pointer. Set to NULL after freeing.
 */
void thread_pool_destroy(thread_pool **pool_ptr);

/**
 * @brief Submit a task to the pool.
 *
 * @param pool The pool instance.
 * @param function The function to execute.
 * @param arg The argument to pass to the function.
 * @return 0 on success, <0 on failure.
 */
int thread_pool_submit(thread_pool *pool, void (*function)(void *), void *arg);

#endif /* HEIMWATT_THREAD_POOL_H */
