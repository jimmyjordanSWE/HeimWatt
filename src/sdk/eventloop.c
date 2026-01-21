#include <errno.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "memory.h"
#include "sdk_eventloop.h"

#define SDK_MAX_FDS 64
#define SDK_MAX_TICKERS 32

typedef struct
{
    int fd;
    int events;
    sdk_fd_callback cb;
    void *ctx;
} eventloop_fd;

typedef struct
{
    int64_t next_run;
    int interval_sec;
    sdk_next_run_fn next_check_fn; /* Optional: if set, overrides interval_sec logic */
    sdk_ticker_callback cb;
    void *ctx;
    bool active;
} eventloop_ticker;

struct sdk_eventloop
{
    struct pollfd poll_fds[SDK_MAX_FDS];
    eventloop_fd handlers[SDK_MAX_FDS];
    int fd_count;

    eventloop_ticker tickers[SDK_MAX_TICKERS];
    int ticker_count;

    atomic_bool running;
};

sdk_eventloop *sdk_eventloop_create(void)
{
    sdk_eventloop *loop = mem_alloc(sizeof(*loop));
    if (!loop) return NULL;

    loop->fd_count = 0;
    loop->ticker_count = 0;
    atomic_init(&loop->running, false);

    return loop;
}

void sdk_eventloop_destroy(sdk_eventloop **loop_ptr)
{
    if (!loop_ptr || !*loop_ptr) return;
    mem_free(*loop_ptr);
    *loop_ptr = NULL;
}

int sdk_eventloop_add_fd(sdk_eventloop *loop, int fd, int events, sdk_fd_callback cb, void *ctx)
{
    if (!loop || fd < 0 || !cb) return -EINVAL;
    if (loop->fd_count >= SDK_MAX_FDS) return -ENOSPC;

    // Check if duplicate? For now, assume unique or allow multiple handlers for same FD?
    // Epoll supports one per FD. Poll supports duplicates but it's weird.
    // Let's assume unique FD registration for now.
    for (int i = 0; i < loop->fd_count; i++)
    {
        if (loop->handlers[i].fd == fd) return -EEXIST;
    }

    int idx = loop->fd_count;
    loop->poll_fds[idx].fd = fd;
    loop->poll_fds[idx].events = (short) events;
    loop->poll_fds[idx].revents = 0;

    loop->handlers[idx].fd = fd;
    loop->handlers[idx].events = events;
    loop->handlers[idx].cb = cb;
    loop->handlers[idx].ctx = ctx;

    loop->fd_count++;
    return 0;
}

int sdk_eventloop_remove_fd(sdk_eventloop *loop, int fd)
{
    if (!loop || fd < 0) return -EINVAL;

    for (int i = 0; i < loop->fd_count; i++)
    {
        if (loop->handlers[i].fd == fd)
        {
            // Move last to current
            int last = loop->fd_count - 1;
            if (i != last)
            {
                loop->poll_fds[i] = loop->poll_fds[last];
                loop->handlers[i] = loop->handlers[last];
            }
            loop->fd_count--;
            return 0;
        }
    }
    return -ENOENT;
}

int sdk_eventloop_add_ticker(sdk_eventloop *loop, int interval_sec, sdk_ticker_callback cb,
                             void *ctx)
{
    if (!loop || interval_sec <= 0 || !cb) return -EINVAL;
    if (loop->ticker_count >= SDK_MAX_TICKERS) return -ENOSPC;

    int idx = loop->ticker_count;
    loop->tickers[idx].interval_sec = interval_sec;
    loop->tickers[idx].cb = cb;
    loop->tickers[idx].ctx = ctx;
    loop->tickers[idx].next_check_fn = NULL;
    loop->tickers[idx].next_run = time(NULL);  // Run immediately? or wait?
    // Usually tickers run at "next alignment" or "immediately".
    // Let's set it to NOW so it runs on first loop, then schedules next.
    loop->tickers[idx].active = true;

    loop->ticker_count++;
    return 0;
}

int sdk_eventloop_add_scheduled_task(sdk_eventloop *loop, sdk_next_run_fn next_run_fn,
                                     sdk_ticker_callback cb, void *ctx)
{
    if (!loop || !next_run_fn || !cb) return -EINVAL;
    if (loop->ticker_count >= SDK_MAX_TICKERS) return -ENOSPC;

    int idx = loop->ticker_count;
    loop->tickers[idx].interval_sec = 0;
    loop->tickers[idx].next_check_fn = next_run_fn;
    loop->tickers[idx].cb = cb;
    loop->tickers[idx].ctx = ctx;
    loop->tickers[idx].active = true;

    // Calculate first run
    loop->tickers[idx].next_run = next_run_fn(ctx, time(NULL));

    loop->ticker_count++;
    return 0;
}

void sdk_eventloop_stop(sdk_eventloop *loop)
{
    if (loop)
    {
        atomic_store(&loop->running, false);
    }
}

int sdk_eventloop_run(sdk_eventloop *loop)
{
    if (!loop) return -EINVAL;

    atomic_store(&loop->running, true);

    while (atomic_load(&loop->running))
    {
        // 1. Calculate timeout
        int timeout_ms = -1;
        int64_t now = time(NULL);
        int64_t next_event = -1;

        for (int i = 0; i < loop->ticker_count; i++)
        {
            if (!loop->tickers[i].active) continue;

            if (next_event == -1 || loop->tickers[i].next_run < next_event)
            {
                next_event = loop->tickers[i].next_run;
            }
        }

        if (next_event != -1)
        {
            int64_t diff = next_event - now;
            if (diff < 0) diff = 0;
            timeout_ms = (int) diff * 1000;
        }

        // 2. Poll
        // We use poll_fds array directly
        int n = poll(loop->poll_fds, loop->fd_count, timeout_ms);

        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -errno;
        }

        // 3. Dispatch FDs
        if (n > 0)
        {
            for (int i = 0; i < loop->fd_count; i++)
            {
                if (loop->poll_fds[i].revents != 0)
                {
                    loop->handlers[i].cb(loop->handlers[i].ctx, loop->poll_fds[i].fd,
                                         loop->poll_fds[i].revents);
                    // Reset revents handled by next poll call
                }
            }
        }

        // 4. Dispatch Tickers
        now = time(NULL);
        for (int i = 0; i < loop->ticker_count; i++)
        {
            if (!loop->tickers[i].active) continue;

            if (now >= loop->tickers[i].next_run)
            {
                // Run
                loop->tickers[i].cb(loop->tickers[i].ctx, now);

                // Schedule next
                if (loop->tickers[i].next_check_fn)
                {
                    loop->tickers[i].next_run =
                        loop->tickers[i].next_check_fn(loop->tickers[i].ctx, now);
                }
                else
                {
                    // Simple interval
                    loop->tickers[i].next_run = now + loop->tickers[i].interval_sec;
                }
            }
        }
    }

    return 0;
}
