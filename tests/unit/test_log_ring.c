#include <stdlib.h>
#include <string.h>

#include "libs/unity/unity.h"
#include "log_ring.h"

// setUp and tearDown are handled by test_runner.c dispatch

void test_ring_buffer_basics(void)
{
    log_ring_init(5);

    log_entry e1 = {.timestamp = 100, .message = "First"};
    log_ring_push(&e1);

    log_entry out[10];
    size_t count = log_ring_get_recent(out, 10);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL_STRING("First", out[0].message);

    log_entry e2 = {.timestamp = 200, .message = "Second"};
    log_ring_push(&e2);

    count = log_ring_get_recent(out, 10);
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL_STRING("First", out[0].message);
    TEST_ASSERT_EQUAL_STRING("Second", out[1].message);
}

void test_ring_buffer_overwrite(void)
{
    // Fill buffer (capacity 5)
    for (int i = 0; i < 5; i++)
    {
        log_entry e = {.timestamp = 1000 + i, .level = i};
        snprintf(e.message, sizeof(e.message), "Msg %d", i);
        log_ring_push(&e);
    }

    log_entry out[10];
    size_t count = log_ring_get_recent(out, 10);
    TEST_ASSERT_EQUAL(5, count);
    TEST_ASSERT_EQUAL_STRING("Msg 0", out[0].message);
    TEST_ASSERT_EQUAL_STRING("Msg 4", out[4].message);

    // Push 6th item, should overwrite Msg 0
    log_entry e6 = {.timestamp = 1005, .message = "Msg 5"};
    log_ring_push(&e6);

    count = log_ring_get_recent(out, 10);
    TEST_ASSERT_EQUAL(5, count);
    TEST_ASSERT_EQUAL_STRING("Msg 1", out[0].message);  // Oldest is now Msg 1
    TEST_ASSERT_EQUAL_STRING("Msg 5", out[4].message);  // Newest is Msg 5
}

void test_ring_buffer_json(void)
{
    char *json = log_ring_to_json(10);
    TEST_ASSERT_NOT_NULL(json);
    // Simple check that it contains json structure
    TEST_ASSERT_NOT_NULL(strstr(json, "\"logs\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"message\":\"Msg 5\""));
    free(json);
}
