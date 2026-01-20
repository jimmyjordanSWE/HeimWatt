# High-Performance Architecture Implementation Plan

## Goal

Transform the HeimWatt server from a blocking, sequential architecture to a high-performance, non-blocking architecture capable of handling many concurrent HTTP clients.

## User Review Required

> [!CAUTION]
> This is a significant architectural change affecting core components. Review carefully.

**Key Decisions:**
1. **epoll vs poll** – Using Linux-specific `epoll` for O(1) event notification (acceptable since target is Linux-only per coding standards)
2. **Request ID correlation** – Adding UUIDs to HTTP-IPC messages to match responses
3. **Memory overhead** – In-memory timestamp index adds ~8 bytes per data point (acceptable trade-off for O(n²) → O(1))

---

## Proposed Changes

### 1. Non-Blocking HTTP Server (Critical)

#### [MODIFY] [http_server.c](file:///home/jimmy/HeimWatt/src/net/http_server.c)

Complete rewrite with epoll-based event loop:

- **Connection State Machine**: Each connection tracks parse state, allowing partial reads
- **epoll multiplexing**: Single thread handles all connections via epoll_wait
- **Non-blocking sockets**: All accept/recv/send set to O_NONBLOCK  
- **Async handler model**: Handler returns immediately; response sent when ready

```c
// New internal structure
typedef struct {
    int fd;
    enum { CONN_READING, CONN_PROCESSING, CONN_WRITING } state;
    char read_buf[4096];
    size_t read_pos;
    http_request req;
    http_response resp;
    char *write_buf;
    size_t write_pos, write_len;
    char request_id[37];  // UUID for bridge correlation
} http_conn;
```

#### [MODIFY] [http_server.h](file:///home/jimmy/HeimWatt/src/net/http_server.h)

Add async handler signature:
```c
// NEW: Async handler - returns 0=done, 1=pending (will call complete later)
typedef int (*http_handler_async_fn)(const http_request *req, http_response *resp, 
                                      const char *request_id, void *ctx);

void http_server_complete_request(http_server *srv, const char *request_id, 
                                   http_response *resp);
```

---

### 2. Async HTTP-IPC Bridge (Critical)

#### [MODIFY] [server.c](file:///home/jimmy/HeimWatt/src/server.c)

Replace blocking bridge with async pending request registry:

```c
// NEW: Pending HTTP requests waiting for IPC response
typedef struct {
    char request_id[37];      // Correlation ID
    int64_t deadline;         // Timeout timestamp
    http_response *resp;      // Pre-allocated response
} pending_request;

struct heimwatt_ctx {
    // ... existing fields ...
    
    // Remove old bridge struct
    
    // New: pending request registry  
    pending_request pending[64];
    int pending_count;
    pthread_mutex_t pending_lock;
};
```

**Flow:**
1. HTTP handler generates UUID, stores in pending registry
2. Sends IPC with `request_id` field
3. Returns immediately (handler returns 1 = pending)
4. IPC loop receives `http_response` with `request_id`
5. Looks up pending request, calls `http_server_complete_request`
6. epoll triggers write when socket ready

---

### 3. File Backend Performance (Critical)

#### [MODIFY] [file_backend.c](file:///home/jimmy/HeimWatt/src/db/file_backend.c)

Add in-memory timestamp index per semantic type:

```c
#include <search.h>  // For hsearch (hash table)

struct db_handle {
    char *base_dir;
    char last_error[256];
    
    // NEW: Per-type timestamp indexes (hash tables)
    struct hsearch_data *ts_indexes[SEM_TYPE_COUNT];
    bool indexes_loaded[SEM_TYPE_COUNT];
};
```

**Changes:**
- `db_open`: Initialize empty hash tables
- `db_insert_tier1`: On insert, add timestamp to hash; O(1) check before file I/O
- `db_query_point_exists_tier1`: Hash lookup instead of file scan
- Lazy loading: First query loads all timestamps from file into hash

#### [MODIFY] [server.c](file:///home/jimmy/HeimWatt/src/server.c) - Add check_data handler

```c
else if (strcmp(cmd->valuestring, "check_data") == 0) {
    cJSON *ts_json = cJSON_GetObjectItem(json, "ts");
    cJSON *sem_json = cJSON_GetObjectItem(json, "sem");
    
    int exists = 0;
    if (ts_json && sem_json) {
        semantic_type sem = (semantic_type)sem_json->valueint;
        int64_t ts = (int64_t)ts_json->valuedouble;
        exists = db_query_point_exists_tier1(ctx->db, sem, ts) > 0;
    }
    
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"exists\":%s}\n", exists ? "true" : "false");
    ipc_conn_send(conn, resp, strlen(resp));
}
```

---

### 4. SDK Buffered IPC Reads (Major)

#### [MODIFY] [sdk/ipc.c](file:///home/jimmy/HeimWatt/src/sdk/ipc.c)

Replace byte-at-a-time with buffered reads matching Core's implementation:

```c
// Add to plugin_ctx in sdk_internal.h
char ipc_buf[4096];
size_t ipc_rpos, ipc_wpos;

// New implementation
int sdk_ipc_recv(plugin_ctx *ctx, char *buf, size_t len) {
    // 1. Check buffer for existing newline
    // 2. If not found, read in bulk
    // 3. Extract line up to newline
    // (Mirror src/core/ipc.c logic)
}
```

---

### 5. SDK Memory Leak Fix (Minor)

#### [MODIFY] [lifecycle.c](file:///home/jimmy/HeimWatt/src/sdk/lifecycle.c)

```c
void sdk_destroy(plugin_ctx **ctx_ptr) {
    // ... existing cleanup ...
    
    // NEW: Free ticker cron expressions
    for (int i = 0; i < ctx->ticker_count; i++) {
        if (ctx->tickers[i].is_cron) {
            free(ctx->tickers[i].cron_expr);
        }
    }
    
    free(ctx);
    *ctx_ptr = NULL;
}
```

---

### 6. Banned Function Fix (Minor)

#### [MODIFY] [plugin_mgr.c](file:///home/jimmy/HeimWatt/src/core/plugin_mgr.c)

Replace `strtok` with `strtok_r`:
```c
char *saveptr;
char *tok = strtok_r(buf, ".", &saveptr);
while (tok && seg_count < 8) {
    segments[seg_count++] = tok;
    tok = strtok_r(NULL, ".", &saveptr);
}
```

---

## Verification Plan

### Automated Tests

**Build Test:**
```bash
cd /home/jimmy/HeimWatt
make clean && make debug
```
Expected: Clean compile with no errors.

**ASAN Check:**
```bash
make run
# Let it start, then Ctrl+C
# ASAN should report no leaks
```

### Manual Verification

**1. Concurrent HTTP Load Test:**
```bash
# Terminal 1: Start server
cd /home/jimmy/HeimWatt && make run

# Terminal 2: Run concurrent requests
for i in {1..50}; do
    curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/api/test &
done
wait
# Expected: All return 404 (no registered endpoint) but NO hangs/crashes
```

**2. Memory Leak Test (requires valgrind):**
```bash
# Build without ASAN first
make clean
make release

# Run with valgrind
valgrind --leak-check=full ./build/bin/server_prototype &
sleep 10
kill $!
# Expected: "definitely lost: 0 bytes" for cron expression fix
```

**3. Performance Comparison:**

Before (baseline - file scan):
```bash
time for i in {1..1000}; do
    echo '{"cmd":"report","type":"temperature","val":20,"ts":'$i'}' | nc -U /tmp/heimwatt.sock
done
```

After (with hash index):
```bash
# Same test - should be significantly faster
```

---

## Summary of Changes

| File | Change Type | Risk |
|------|-------------|------|
| `http_server.c` | Rewrite | High |
| `http_server.h` | API addition | Medium |
| `server.c` | Bridge refactor + check_data | High |
| `file_backend.c` | Add index | Medium |
| `sdk/ipc.c` | Buffered reads | Low |
| `lifecycle.c` | Memory fix | Low |
| `plugin_mgr.c` | strtok_r | Low |
