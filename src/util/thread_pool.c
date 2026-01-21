#define _GNU_SOURCE
#include "thread_pool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "mem.h"  // For mem_alloc, mem_free

#define MAX_QUEUE_SIZE 256

typedef struct task
{
    void (*function)(void *);
    void *arg;
    struct task *next;
} task;

struct thread_pool
{
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    task *queue_head;
    task *queue_tail;
    int thread_count;
    int queue_size;
    int shutdown;
    int started;
};

static void *thread_worker(void *arg)
{
    thread_pool *pool = (thread_pool *) arg;

    while (1)
    {
        pthread_mutex_lock(&pool->lock);

        while (pool->queue_size == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }

        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        // Grab task
        task *t = pool->queue_head;
        if (t)
        {
            pool->queue_head = t->next;
            if (pool->queue_head == NULL) pool->queue_tail = NULL;
            pool->queue_size--;
        }

        pthread_mutex_unlock(&pool->lock);

        // Execute task
        if (t)
        {
            (*(t->function))(t->arg);
            mem_free(t);
        }
    }
    return NULL;
}

thread_pool *thread_pool_create(int worker_count)
{
    if (worker_count <= 0 || worker_count > 64) return NULL;

    thread_pool *pool = mem_alloc(sizeof(thread_pool));
    if (!pool) return NULL;

    pool->thread_count = worker_count;
    pool->queue_size = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->shutdown = 0;
    pool->started = 0;

    // Initialize mutex/cond
    if (pthread_mutex_init(&pool->lock, NULL) != 0 || pthread_cond_init(&pool->notify, NULL) != 0)
    {
        mem_free(pool);
        return NULL;
    }

    pool->threads = mem_alloc(sizeof(pthread_t) * worker_count);
    if (!pool->threads)
    {
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
        mem_free(pool);
        return NULL;
    }

    // Start threads
    for (int i = 0; i < worker_count; i++)
    {
        if (pthread_create(&pool->threads[i], NULL, thread_worker, pool) != 0)
        {
            thread_pool_destroy(&pool);
            return NULL;
        }
        pool->started++;
    }

    return pool;
}

int thread_pool_submit(thread_pool *pool, void (*function)(void *), void *arg)
{
    if (!pool || !function) return -1;

    task *t = mem_alloc(sizeof(task));
    if (!t) return -1;

    t->function = function;
    t->arg = arg;
    t->next = NULL;

    pthread_mutex_lock(&pool->lock);

    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->lock);
        mem_free(t);  // Don't accept tasks during shutdown
        return -1;
    }

    // Bounds check? for now optional, implementation plan didn't specify strict bounds but good
    // practice if (pool->queue_size > MAX_QUEUE_SIZE) ...

    if (pool->queue_size == 0)
    {
        pool->queue_head = t;
        pool->queue_tail = t;
        pthread_cond_signal(&pool->notify);
    }
    else
    {
        pool->queue_tail->next = t;
        pool->queue_tail = t;
        // Optimization: only signal if we have idle threads?
        // Or always signal.
        pthread_cond_signal(&pool->notify);
    }
    pool->queue_size++;

    pthread_mutex_unlock(&pool->lock);
    return 0;
}

void thread_pool_destroy(thread_pool **pool_ptr)
{
    if (!pool_ptr || !*pool_ptr) return;
    thread_pool *pool = *pool_ptr;

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    // Join threads
    for (int i = 0; i < pool->thread_count; i++)
    {
        if (i < pool->started)
        {
            pthread_join(pool->threads[i], NULL);
        }
    }

    // Free remaining tasks
    pthread_mutex_lock(&pool->lock);
    task *cur = pool->queue_head;
    while (cur)
    {
        task *next = cur->next;
        mem_free(cur);
        cur = next;
    }
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pthread_mutex_unlock(&pool->lock);

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify);

    mem_free(pool->threads);
    mem_free(pool);
    *pool_ptr = NULL;
}
