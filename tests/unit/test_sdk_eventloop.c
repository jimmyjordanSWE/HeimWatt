#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libs/unity/unity.h"
#include "sdk_eventloop.h"

// setUp and tearDown are in test_runner.c

static void test_fd_callback(void *ctx, int fd, int events)
{
    (void) events;
    int *count = (int *) ctx;
    (*count)++;

    // Read to clear POLLIN
    char buf[16];
    read(fd, buf, sizeof(buf));
}

static void test_ticker_callback(void *ctx, int64_t now)
{
    int *count = (int *) ctx;
    (*count)++;
    (void) now;
}

static void test_stop_callback(void *ctx, int64_t now)
{
    sdk_eventloop *loop = (sdk_eventloop *) ctx;
    sdk_eventloop_stop(loop);
    (void) now;
}

void test_sdk_eventloop_create_destroy(void)
{
    sdk_eventloop *loop = sdk_eventloop_create();
    TEST_ASSERT_NOT_NULL(loop);
    sdk_eventloop_destroy(&loop);
    TEST_ASSERT_NULL(loop);
}

void test_sdk_eventloop_fd(void)
{
    sdk_eventloop *loop = sdk_eventloop_create();
    int pair[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, pair);

    // Non-blocking
    fcntl(pair[0], F_SETFL, O_NONBLOCK);
    fcntl(pair[1], F_SETFL, O_NONBLOCK);

    int call_count = 0;
    sdk_eventloop_add_fd(loop, pair[0], POLLIN, test_fd_callback, &call_count);

    // Write something
    write(pair[1], "x", 1);

    // Create a stopper ticker to exit loop after brief time (1s) if it doesn't return
    // But sdk_eventloop_run only returns when stopped.
    // We need a way to stop it.
    // We can add a ticker that stops the loop.
    sdk_eventloop_add_ticker(loop, 1, test_stop_callback, loop);

    // Since our implementation runs tickers immediately on start,
    // the stop callback might run immediately if we pass interval.
    // Implementation: "loop->tickers[idx].next_run = time(NULL);"
    // So it runs immediately.
    // We want it to run AFTER the FD event processing if possible.
    // Poll happens first. If FD is ready, it handles it.
    // BUT we need to ensure poll returns positive.

    // Let's run.
    sdk_eventloop_run(loop);

    TEST_ASSERT_EQUAL_INT(1, call_count);  // Should have triggered once

    sdk_eventloop_destroy(&loop);
    close(pair[0]);
    close(pair[1]);
}

void test_sdk_eventloop_ticker(void)
{
    sdk_eventloop *loop = sdk_eventloop_create();
    int call_count = 0;

    // Ticker that runs immediately and increments count
    sdk_eventloop_add_ticker(loop, 100, test_ticker_callback, &call_count);

    // Stopper runs immediately too, but let's see order.
    // If indices are preserved, first added runs first?
    // Implementation iterates 0..ticker_count.
    // So added FIRST runs FIRST.
    // We want the counter to run, THEN the stopper.
    // But stopper needs access to loop.

    sdk_eventloop_add_ticker(loop, 1, test_stop_callback, loop);

    sdk_eventloop_run(loop);

    TEST_ASSERT_GREATER_THAN(0, call_count);

    sdk_eventloop_destroy(&loop);
}

void test_sdk_eventloop_remove_fd(void)
{
    sdk_eventloop *loop = sdk_eventloop_create();
    int fd = 10;

    TEST_ASSERT_EQUAL_INT(0, sdk_eventloop_add_fd(loop, fd, POLLIN, test_fd_callback, NULL));
    TEST_ASSERT_EQUAL_INT(0, sdk_eventloop_remove_fd(loop, fd));
    TEST_ASSERT_EQUAL_INT(-ENOENT, sdk_eventloop_remove_fd(loop, fd));

    sdk_eventloop_destroy(&loop);
}
