# Implementation Plan 5: IPC Epoll Unification

> **Priority**: 5 (Do Last)  
> **Effort**: High  
> **Risk**: Medium-High (changes core event handling)

---

## Overview

Unify the IPC server with the HTTP server's epoll-based event model. This eliminates the nested per-connection loop in `heimwatt_run_with_shutdown_flag` and creates a consistent event-driven architecture.

---

## Current State

```c
// server.c - Two different event models
// HTTP: epoll-based (in http_server.c)
// IPC: poll-based with explicit loop

while (running) {
    // Set up poll fds manually
    fds[0].fd = ipc_server_fd(ctx->ipc);
    for (int i = 0; i < ctx->conn_count; i++) {      // L2: Per-connection loop
        fds[nfds].fd = ipc_conn_fd(ctx->conns[i]);
    }
    
    poll(fds, nfds, 1000);
    
    // Handle each connection explicitly
    for (int i = 0; i < ctx->conn_count; i++) {      // Another per-connection loop
        // Handle events...
    }
}
```

---

## Target State

```c
// server.c - Unified epoll model
// Both HTTP and IPC use the same pattern

while (running) {
    int n = epoll_wait(ctx->epoll_fd, events, MAX_EVENTS, 1000);
    
    for (int i = 0; i < n; i++) {
        void *ptr = events[i].data.ptr;
        
        if (ptr == IPC_LISTEN_MARKER) {
            ipc_accept_connection(ctx);
        } else if (is_ipc_conn(ptr)) {
            ipc_handle_event(ctx, ptr, events[i].events);
        }
        // HTTP events handled by http_server thread
    }
    
    db_tick(ctx->db);
    plugin_mgr_check_health(ctx->plugins);
}
```

---

## Architecture Decision

### Option A: Shared Epoll FD

Both HTTP and IPC share the same epoll instance. The main loop handles both.

```
┌─────────────────────────────────────────┐
│              Main Thread                │
│  ┌─────────────────────────────────┐    │
│  │         epoll_wait()            │    │
│  │   - IPC listen socket           │    │
│  │   - IPC client connections      │    │
│  │   - HTTP handled separately     │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

### Option B: Separate Epoll, Unified Pattern (Recommended)

Keep HTTP in its thread, move IPC to epoll in main thread.

```
┌────────────────────┐    ┌────────────────────┐
│   Main Thread      │    │   HTTP Thread      │
│   (IPC epoll)      │    │   (HTTP epoll)     │
│                    │    │                    │
│ epoll_wait() for:  │    │ epoll_wait() for:  │
│ - IPC listen       │    │ - HTTP listen      │
│ - IPC connections  │    │ - HTTP connections │
└────────────────────┘    └────────────────────┘
```

**Recommendation**: Option B - It's less invasive and keeps the HTTP server self-contained.

---

## Tasks

### 5.1 Add Epoll to IPC Server

Modify `src/core/ipc.c`:

```c
struct ipc_server {
    int listen_fd;
    int epoll_fd;       // NEW
    // ...
};

int ipc_server_init(ipc_server **srv_out, const char *socket_path) {
    // ... existing code ...
    
    // Create epoll
    srv->epoll_fd = epoll_create1(0);
    
    // Add listen socket
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = srv};
    epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, srv->listen_fd, &ev);
}

// New function
int ipc_server_poll(ipc_server *srv, struct epoll_event *events, 
                    int max_events, int timeout_ms) {
    return epoll_wait(srv->epoll_fd, events, max_events, timeout_ms);
}
```

### 5.2 Track Connections via Epoll

```c
int ipc_server_accept(ipc_server *srv, ipc_conn **conn_out) {
    // ... existing accept code ...
    
    // Add to epoll
    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLET,
        .data.ptr = conn
    };
    epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, conn->fd, &ev);
}

void ipc_conn_destroy(ipc_conn **conn_ptr) {
    // Remove from epoll before closing
    epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    // ... existing cleanup ...
}
```

### 5.3 Refactor Main Loop

```c
// server.c - New main loop structure
void heimwatt_run_with_shutdown_flag(heimwatt_ctx *ctx, volatile sig_atomic_t *shutdown_flag) {
    struct epoll_event events[MAX_EVENTS];
    
    while (atomic_load(&ctx->running) && (!shutdown_flag || !*shutdown_flag)) {
        int n = ipc_server_poll(ctx->ipc, events, MAX_EVENTS, 1000);
        
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        for (int i = 0; i < n; i++) {
            void *ptr = events[i].data.ptr;
            
            if (ptr == ctx->ipc) {
                // Accept new connection
                ipc_conn *new_conn = NULL;
                if (ipc_server_accept(ctx->ipc, &new_conn) == 0) {
                    register_connection(ctx, new_conn);
                }
            } else {
                // Handle existing connection
                ipc_conn *conn = (ipc_conn *)ptr;
                handle_ipc_event(ctx, conn, events[i].events);
            }
        }
        
        // Periodic tasks
        db_tick(ctx->db);
        plugin_mgr_check_health(ctx->plugins);
    }
}
```

### 5.4 Handle Connection Events

```c
static void handle_ipc_event(heimwatt_ctx *ctx, ipc_conn *conn, uint32_t events) {
    if (events & (EPOLLERR | EPOLLHUP)) {
        disconnect_plugin(ctx, conn);
        return;
    }
    
    if (events & EPOLLOUT) {
        ipc_conn_flush(conn);
    }
    
    if (events & EPOLLIN) {
        char *msg = NULL;
        size_t len = 0;
        
        if (ipc_conn_recv(conn, &msg, &len) < 0) {
            disconnect_plugin(ctx, conn);
            return;
        }
        
        // Parse and handle messages (no loop - edge-triggered)
        process_ipc_messages(ctx, conn, msg, len);
        free(msg);
    }
}
```

### 5.5 Update IPC API

Add to `include/ipc.h`:

```c
// Epoll-based polling
int ipc_server_get_epoll_fd(const ipc_server *srv);
int ipc_server_poll(ipc_server *srv, struct epoll_event *events, 
                    int max_events, int timeout_ms);
```

---

## Migration Strategy

1. **Phase 1**: Add epoll FD to `ipc_server` (backward compatible)
2. **Phase 2**: Add `ipc_server_poll()` function
3. **Phase 3**: Refactor main loop to use epoll
4. **Phase 4**: Remove old poll-based code
5. **Phase 5**: Test thoroughly

---

## Verification

```bash
# Build
make clean && make

# Run with multiple plugins
make run

# Stress test - many connections
for i in {1..20}; do
    ./build/bin/plugins/smhi_weather --socket /tmp/heimwatt.sock --id test$i &
done

# Check all plugins register correctly
curl http://localhost:8080/api/plugins | jq '.plugins | length'
# Should show 20

# Kill and restart plugins - should reconnect cleanly
killall smhi_weather
sleep 2
./build/bin/plugins/smhi_weather --socket /tmp/heimwatt.sock --id test1 &
```

---

## Completion Criteria

- [ ] IPC uses epoll for event dispatch
- [ ] No explicit per-connection loop in main loop
- [ ] Plugin connect/disconnect works correctly
- [ ] Multiple plugins work simultaneously
- [ ] No performance regression
- [ ] Edge-triggered mode handles partial reads correctly

---

## Dependencies

- Strongly Recommended: [Plan 2: IPC Dispatch](impl_plan_2_ipc_dispatch.md) (cleaner message handling)
- Recommended: [Plan 4: SDK Event Loop](impl_plan_4_sdk_eventloop.md) (consistent patterns)

---

## Risk Mitigation

1. **Edge-triggered complexity**: Edge-triggered epoll requires careful handling of partial reads. Ensure `ipc_conn_recv` loops until `EAGAIN`.

2. **Connection lifecycle**: Epoll events may arrive after connection is freed. Use reference counting (already in http_server pattern).

3. **Thread safety**: Main loop is single-threaded for IPC, so no mutex needed for connection list.

---

## Summary

This is the most complex change, but it completes the architectural unification. After this:
- HTTP and IPC both use epoll
- Event-driven throughout
- No nested connection loops
- Consistent patterns across codebase
