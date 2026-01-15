# LLM Code Review Report

## Summary
The codebase demonstrates solid adherence to C idioms, opaque pointer patterns, and resource management pairing. However, there are **critical architectural flaws** related to concurrency and IPC that jeopardize the "High-Performance" and "Robustness" goals.

## 1. Concurrency & Race Conditions

### [CRITICAL] Race Condition in Async HTTP Completion
**Location**: `src/net/http_server.c`
**Severity**: **Critical** (Potential Use-After-Free / Data Corruption)

There is a race condition between a client disconnecting (`close_connection`) and a plugin returning a response (`http_server_complete_request`).

1.  **Thread A (IPC)** calls `http_server_complete_request`. It locks `pending_lock`, finds the `conn`, removes it from the pending list, and **UNLOCKS** (Line 240).
2.  **Thread B (HTTP)** handles an event (e.g., `EPOLLERR` or timeout) and calls `close_connection`. It locks `pending_lock`, sees the connection is NOT in the pending list (already removed by Thread A), and proceeds to `conn_free` (Line 711).
3.  **Thread B** calls `conn_free`, which adds `conn` to the `free_list` (Line 467).
4.  **Thread A** resumes and writes to `conn` (Line 249: `memcpy`, Line 256: write buf).
    *   **Result**: Thread A is writing to freed memory (or memory now assigned to a NEW connection).

**Fix**: The `conn` lifecycle must be shared or reference-counted, OR `http_server_complete_request` must hold a lock that prevents `close_connection` from freeing the memory until it is done.

### [CRITICAL] Blocking IPC I/O causing Global Server Hang
**Location**: `src/core/ipc.c` & `src/server.c`
**Severity**: **Critical** (Denial of Service)

The IPC mechanism uses **blocking sockets** (default for `socketpair`/`accept` without `O_NONBLOCK`).

1.  **Sending**: `ipc_conn_send` uses blocking `write()`. In `src/server.c:126`, this is called while holding the global `ctx->conn_lock`.
    *   **Impact**: If a plugin is slow to read or stuck, the `write()` blocks. Since `conn_lock` is held, **ALL** HTTP threads block when trying to route requests (Line 83), and the Main Thread blocks when accepting/cleaning up connections. One bad plugin halts the entire server.
2.  **Receiving**: `ipc_conn_recv` (Line 119) loops until a newline is found.
    *   **Impact**: If a plugin sends a partial line and stops, the Main Thread (which calls `ipc_conn_recv`) will **BLOCK indefinitely** inside the loop waiting for the rest of the line. This freezes the entire event loop (`heimwatt_run`).

**Fix**:
*   Make IPC sockets non-blocking (`O_NONBLOCK`).
*   Implement a state machine for `ipc_conn_send`/`recv` to handle partial reads/writes without blocking.
*   Use `epoll` for IPC writes (buffer data in userspace, write when `EPOLLOUT`).

## 2. Architecture & Design

### Opaque Pointers & Encapsulation
**Status**: **Excellent**
The codebase consistently uses opaque pointers (`heimwatt_ctx`, `http_server`, `plugin_mgr`) and pairs `create/destroy` correctly. This adheres well to `docs/coding_standards.md`.

### Error Handling
**Status**: **Good**
The use of negative `errno` values for error returns is consistent and Idiomatic.

## 3. Resource Management

### Cleanup Logic
**Status**: **Good**
`goto cleanup` patterns in `server.c:heimwatt_init` correctly ensure resources are released on failure.

### Memory Safety
**Status**: **Mixed**
*   **Good**: `heimwatt_destroy` nullifies the caller's pointer.
*   **Risk**: The use-after-free in `http_server.c` (mentioned above) is the primary memory safety risk.

## Recommendations

1.  **Implement Reference Counting for `http_conn`**: This ensures a connection cannot be freed while an async request is holding a reference to it.
2.  **Rewrite IPC to be Non-Blocking**: The IPC layer must never block the main thread or hold locks while blocking. Use ring buffers or existing `recv/send` buffering with state machines.
3.  **Reduce/Refine Locks**: `ctx->conn_lock` is too coarse if it covers IPC sending. It should only protect map lookups.
