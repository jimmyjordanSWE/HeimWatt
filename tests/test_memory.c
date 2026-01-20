#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"

// Mock logging or just use printf
#define LOG(...) printf(__VA_ARGS__)

void test_mem_alloc(void)
{
    LOG("Testing mem_alloc...\n");
    int *p = mem_alloc(sizeof(int));
    assert(p != NULL);
    assert(*p == 0);  // Check zero init

    *p = 123;

    int *q = mem_alloc(sizeof(*q));
    assert(q != NULL);
    assert(*q == 0);  // Check zero init

    mem_free(p);
    mem_free(q);
    LOG("PASS: mem_alloc\n");
}

void test_hw_pool(void)
{
    LOG("Testing HwPool...\n");
    // Pool of 2 integers
    HwPool *pool = hw_pool_create(sizeof(int), 2);
    assert(pool != NULL);

    int *p1 = hw_pool_alloc(pool);
    assert(p1 != NULL);
    assert(*p1 == 0);
    *p1 = 42;

    int *p2 = hw_pool_alloc(pool);
    assert(p2 != NULL);
    assert(*p2 == 0);
    *p2 = 84;

    // Pool should be empty now
    int *p3 = hw_pool_alloc(pool);
    assert(p3 == NULL);

    // Return p1
    hw_pool_free(pool, p1);

    // Allocate again, should get a zeroed pointer (reuse)
    int *p4 = hw_pool_alloc(pool);
    assert(p4 == p1);  // LIFO behavior usually
    assert(*p4 == 0);  // Must be zeroed again!

    hw_pool_destroy(&pool);
    assert(pool == NULL);
    LOG("PASS: HwPool\n");
}

void test_hw_arena(void)
{
    LOG("Testing HwArena...\n");
    // Small block size to force multiple blocks
    HwArena *arena = hw_arena_create(256);
    assert(arena != NULL);

    void *p1 = hw_arena_alloc(arena, 100);
    assert(p1 != NULL);
    // Check zeroing
    char *c1 = p1;
    for (int i = 0; i < 100; i++) assert(c1[i] == 0);
    memset(p1, 0xAA, 100);  // Dirty it

    void *p2 = hw_arena_alloc(arena, 100);
    assert(p2 != NULL);
    assert(p2 > p1);  // Should be sequential

    // Reset
    hw_arena_reset(arena);

    // Alloc again - should reuse first block
    void *p3 = hw_arena_alloc(arena, 100);
    assert(p3 == p1);  // Should reuse space

    // Check it's zeroed again (critical!)
    char *c3 = p3;
    for (int i = 0; i < 100; i++) assert(c3[i] == 0);

    hw_arena_destroy(&arena);
    assert(arena == NULL);
    LOG("PASS: HwArena\n");
}

int main(void)
{
    test_mem_alloc();
    test_hw_pool();
    test_hw_arena();
    return 0;
}
