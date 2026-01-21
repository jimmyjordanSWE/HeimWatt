#include <stdatomic.h>
#include <unistd.h>

#include "libs/unity/unity.h"
#include "util/mem.h"
#include "util/thread_pool.h"

static atomic_int counter;

static void reset_counter(void) { atomic_store(&counter, 0); }

static void increment_task(void *arg)
{
    (void) arg;
    atomic_fetch_add(&counter, 1);
}

static void slow_task(void *arg)
{
    int *val = (int *) arg;
    usleep(50000);  // 50ms
    atomic_fetch_add(&counter, *val);
    mem_free(val);
}

void test_thread_pool_create_destroy(void)
{
    reset_counter();
    thread_pool *pool = thread_pool_create(4);
    TEST_ASSERT_NOT_NULL(pool);
    thread_pool_destroy(&pool);
    TEST_ASSERT_NULL(pool);
}

void test_thread_pool_submit_simple(void)
{
    reset_counter();
    thread_pool *pool = thread_pool_create(2);
    TEST_ASSERT_NOT_NULL(pool);

    int ret = thread_pool_submit(pool, increment_task, NULL);
    TEST_ASSERT_EQUAL(0, ret);

    // Give it time to process
    usleep(100000);

    TEST_ASSERT_EQUAL(1, atomic_load(&counter));

    thread_pool_destroy(&pool);
}

void test_thread_pool_concurrency(void)
{
    reset_counter();
    thread_pool *pool = thread_pool_create(4);

    // Submit 10 slow tasks
    for (int i = 0; i < 10; i++)
    {
        int *arg = mem_alloc(sizeof(int));
        *arg = 1;
        thread_pool_submit(pool, slow_task, arg);
    }

    // Since we have 4 threads, 10 tasks (50ms each) should take ~150ms total, not 500ms
    usleep(200000);

    // Should be done
    TEST_ASSERT_EQUAL(10, atomic_load(&counter));

    thread_pool_destroy(&pool);
}

void test_thread_pool_invalid_args(void)
{
    TEST_ASSERT_NULL(thread_pool_create(0));
    TEST_ASSERT_NULL(thread_pool_create(-1));

    thread_pool *pool = thread_pool_create(1);
    TEST_ASSERT_EQUAL(-1, thread_pool_submit(NULL, increment_task, NULL));
    TEST_ASSERT_EQUAL(-1, thread_pool_submit(pool, NULL, NULL));
    thread_pool_destroy(&pool);
}
