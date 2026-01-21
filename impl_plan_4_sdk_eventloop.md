# Implementation Plan 4: SDK Event Loop Module

> **Priority**: 4  
> **Effort**: Medium-High  
> **Risk**: Medium (new abstraction, but well-defined scope)

---

## Overview

Extract the event loop logic from `sdk/lifecycle.c` (269 LOC `sdk_run` function) into a dedicated `sdk_eventloop` module. This mirrors the pattern used by `tcp_server.c` and `http_server.c`.

---

## Current State

```c
// lifecycle.c - sdk_run handles everything:
// - Backfill mode
// - Poll setup
// - Timeout calculation
// - IPC message handling
// - HTTP request dispatch
// - FD callbacks
// - Ticker scheduling
int sdk_run(plugin_ctx *ctx) {
    // 269 lines of mixed concerns
}
```

---

## Target State

```c
// lifecycle.c - focused on lifecycle
int sdk_run(plugin_ctx *ctx) {
    if (sdk_ipc_connect(ctx) < 0) return -1;
    
    sdk_eventloop *loop = sdk_eventloop_create();
    
    // Configure
    sdk_eventloop_set_ipc_handler(loop, sdk_handle_ipc, ctx);
    sdk_eventloop_set_ticker_handler(loop, sdk_handle_ticker, ctx);
    
    // Register FDs
    sdk_eventloop_add_fd(loop, ctx->ipc_fd, POLLIN);
    
    // Optionally run backfill first
    if (ctx->in_backfill_mode) {
        sdk_run_backfill(ctx, loop);
    }
    
    // Main loop
    int ret = sdk_eventloop_run(loop);
    
    sdk_eventloop_destroy(&loop);
    return ret;
}

// eventloop.c - focused on event dispatch
int sdk_eventloop_run(sdk_eventloop *loop) {
    while (loop->running) {
        int timeout = calculate_next_timeout(loop);
        int n = poll(loop->fds, loop->fd_count, timeout);
        
        dispatch_events(loop, n);
        dispatch_tickers(loop);
    }
    return 0;
}
```

---

## Tasks

### 4.1 Create Event Loop Module

| File | Purpose |
|------|---------|
| `src/sdk/eventloop.c` | Event loop implementation |
| `include/sdk_eventloop.h` | Public API (SDK-internal) |

### 4.2 Define Event Loop API

```c
// sdk_eventloop.h
typedef struct sdk_eventloop sdk_eventloop;

// Lifecycle
sdk_eventloop *sdk_eventloop_create(void);
void sdk_eventloop_destroy(sdk_eventloop **loop);

// Configuration
typedef void (*sdk_fd_callback)(void *ctx, int fd, int events);
typedef void (*sdk_ticker_callback)(void *ctx, int64_t now);

int sdk_eventloop_add_fd(sdk_eventloop *loop, int fd, int events, 
                          sdk_fd_callback cb, void *ctx);
int sdk_eventloop_remove_fd(sdk_eventloop *loop, int fd);

int sdk_eventloop_add_ticker(sdk_eventloop *loop, int interval_sec,
                              sdk_ticker_callback cb, void *ctx);

// Control
int sdk_eventloop_run(sdk_eventloop *loop);   // Blocking
void sdk_eventloop_stop(sdk_eventloop *loop);
```

### 4.3 Implement Event Loop

```c
// eventloop.c
struct sdk_eventloop {
    struct pollfd fds[SDK_MAX_FDS];
    struct {
        sdk_fd_callback cb;
        void *ctx;
    } fd_handlers[SDK_MAX_FDS];
    int fd_count;
    
    struct {
        int interval_sec;
        int64_t next_run;
        sdk_ticker_callback cb;
        void *ctx;
    } tickers[SDK_MAX_TICKERS];
    int ticker_count;
    
    atomic_bool running;
};

int sdk_eventloop_run(sdk_eventloop *loop) {
    atomic_store(&loop->running, true);
    
    while (atomic_load(&loop->running)) {
        int timeout = eventloop_calculate_timeout(loop);
        int n = poll(loop->fds, loop->fd_count, timeout);
        
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        // Dispatch FD events
        for (int i = 0; i < loop->fd_count && n > 0; i++) {
            if (loop->fds[i].revents) {
                loop->fd_handlers[i].cb(
                    loop->fd_handlers[i].ctx,
                    loop->fds[i].fd,
                    loop->fds[i].revents
                );
                n--;
            }
        }
        
        // Dispatch tickers
        int64_t now = time(NULL);
        for (int i = 0; i < loop->ticker_count; i++) {
            if (now >= loop->tickers[i].next_run) {
                loop->tickers[i].cb(loop->tickers[i].ctx, now);
                loop->tickers[i].next_run = now + loop->tickers[i].interval_sec;
            }
        }
    }
    
    return 0;
}
```

### 4.4 Refactor lifecycle.c

1. Extract backfill logic to `sdk_run_backfill()`
2. Extract IPC message handling to `sdk_handle_ipc_event()`
3. Use `sdk_eventloop` for main loop
4. Keep `sdk_run()` as the entry point

### 4.5 Update Makefile

Add `src/sdk/eventloop.c` to SDK build.

---

## Proposed File Structure

```
src/sdk/
├── lifecycle.c        # sdk_create, sdk_destroy, sdk_run (simplified)
├── eventloop.c        # NEW: Generic event loop
├── ipc.c              # IPC helpers (unchanged)
├── config.c           # Config helpers (unchanged)
└── ...

include/
├── heimwatt_sdk.h     # Public SDK API (unchanged)
├── sdk_eventloop.h    # NEW: Event loop API (SDK-internal)
└── ...
```

---

## Verification

```bash
# Build SDK and plugins
make clean && make

# Run a plugin standalone (should work as before)
./build/bin/plugins/smhi_weather --socket /tmp/test.sock --id test

# Run full system
make run

# Verify plugins work correctly
curl http://localhost:8080/api/plugins
```

---

## Completion Criteria

- [ ] `sdk_run()` reduced to < 50 LOC
- [ ] `sdk_eventloop` module is self-contained
- [ ] All plugins work correctly
- [ ] No memory leaks
- [ ] Event loop can be unit tested independently

---

## Dependencies

- Requires: [Plan 2: IPC Dispatch](impl_plan_2_ipc_dispatch.md) (recommended for cleaner integration)
- Independent of: [Plan 3: Logging](impl_plan_3_logging.md)

---

## Next Plan

After completing this, proceed to [Implementation Plan 5: IPC Epoll Unification](impl_plan_5_ipc_epoll.md).
