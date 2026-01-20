# Memory Management Manual

## Overview

HeimWatt employs a **multi-tiered memory management system** designed for safety, performance, and predictability. Instead of relying solely on the system allocator (`malloc`/`free`), we use a unified interface (`mem_alloc`) backed by specialized strategies for different object lifecycles.

## Core Hierarchy

| Layer | Component | Use Case | Benefits |
|-------|-----------|----------|----------|
| **1. Unified** | `mem_alloc` | General purpose, permanent objects. | Tracking, zero-initialization, safety checks. |
| **2. Pool** | `HwPool` | High-frequency, fixed-size objects (e.g., Connections). | O(1) alloc/free, no fragmentation, throttling. |
| **3. Arena** | `HwArena` | Request-scoped data (e.g., Parsed HTTP/JSON). | Zero-overhead cleanup, cache locality. |
| **4. Utility** | `HwBuffer` | Growable dynamic arrays (Strings, Buffers). | Automatic growth, safe resizing. |

---

## 1. Unified Allocator (`mem_alloc`)

The foundation of the system. **Raw `malloc`, `calloc`, and `free` are BANNED.**

### Features
*   **Zero-Initialization**: All memory is cleared upon allocation.
*   **Tracking**: (Debug builds) Tracks leakage and usage statistics.
*   **Error Safety**: Handles 0-size allocations gracefully.

### API
```c
#include "memory.h"

// Allocation (Always Zeroed, refactor-safe)
my_struct *obj = mem_alloc(sizeof(*obj));

// Arrays
int *arr = mem_alloc(count * sizeof(*arr));

// Cleanup
mem_free(obj);
```

---

## 2. Object Pools (`HwPool`)

Used for objects that are created and destroyed frequently (thousands/sec) but have a fixed size.

### Why Pool?
*   **Performance**: Allocation is just a pointer increment/swap (nanoseconds).
*   **Fragmentation**: Prevents heap fragmentation from frequent alloc/free cycles.
*   **Backpressure**: A pool has a fixed global size (e.g., 10,000 connections), acting as a natural rate limiter.

### Example: Connection Pooling
```c
// Initialization (At startup)
HwPool *conn_pool = hw_pool_create(sizeof(http_conn), 10000);

// Usage (Hot Path)
http_conn *conn = hw_pool_alloc(conn_pool);
if (conn) {
    // ... use connection ...
    hw_pool_free(conn_pool, conn);
}
```

### Pool Exhaustion (Backpressure)

When a pool is full, `hw_pool_alloc` returns **NULL immediately** (non-blocking).

**Why not block?**
- HeimWatt uses an event-driven architecture (epoll). Blocking would freeze the entire server.
- Blocking risks deadlock if all workers wait on a pool no one is returning to.

**Caller Responsibility:**
```c
http_conn *conn = hw_pool_alloc(pool);
if (!conn) {
    // Pool exhausted - reject request gracefully
    log_warn("[HTTP] Connection pool exhausted");
    return send_503_service_unavailable(client_fd);
}
```

This design provides **natural load shedding**: under heavy load, new requests are rejected rather than queueing indefinitely.

### Observability (Metrics)

Pools track exhaustion events for monitoring:

```c
// Get number of times pool was empty
size_t exhausts = hw_pool_get_exhaust_count(conn_pool);
log_info("[METRICS] Connection pool exhausted %zu times", exhausts);
```

You can expose this via your admin API (e.g., `/admin/metrics`) to monitor pool health under load.

---

## 3. Scope Arenas (`HwArena`)

Used for objects that belong to a specific unit of work (like an HTTP Request) and can be discarded all at once.

### Why Arena?
*   **Bulk Deallocation**: Free 10,000 tiny strings in O(1) time by resetting the arena.
*   **Simplicity**: No need to track individual `free()` for every struct member.
*   **JSON Parsing**: We use a TLS (Thread-Local) override to force `cJSON` to use the arena during parsing.

### Example: Request Handling
```c
// 1. Create Arena for the request
HwArena *req_arena = hw_arena_create(4096);

// 2. Allocate wildly (No matching free needed)
char *url_copy = hw_arena_alloc(req_arena, strlen(url) + 1);
json_value *json = json_parse_arena(body, req_arena);

// 3. Destroy Arena (Frees EVERYTHING above)
hw_arena_destroy(&req_arena);
```

---

## 4. Dynamic Buffers (`HwBuffer`)

Used for data of unknown or changing size, such as building strings, reading sockets, or serializing responses.

### Features
*   **Geometric Growth**: Doubles capacity to allow O(1) amortized appends.
*   **Null-Termination**: Always ensures safeguards for C-string compatibility.
*   **Reuse**: `hw_buffer_clear` keeps the allocated memory for the next operation.

### usage
```c
HwBuffer buf;
hw_buffer_init(&buf);

hw_buffer_append(&buf, "Hello", 5);
hw_buffer_append(&buf, " World", 6);

printf("%s", buf.data); // "Hello World"

hw_buffer_free(&buf);
```

## Best Practices

1.  **Prefer Pointers**: Pass objects by pointer.
2.  **Clear Ownership**: Know who frees the memory.
    *   **Pool**: The pool owner destroys the pool; users return items.
    *   **Arena**: The request handler destroys the arena; items are abandoned.
    *   **Alloc**: Explicit `create`/`destroy` pairs.
3.  **No hidden malloc**: Functions that allocate must clearly state ownership transfer (usually via return value).