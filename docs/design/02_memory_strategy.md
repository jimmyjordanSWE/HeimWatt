# Design Study: Memory Management & Buffer Strategy

> **Status**: Draft  
> **Date**: 2026-01-20

## The Problem

Fixed buffer sizes (`char buf[4096]`) everywhere. Larger buffers just kicks the can.

---

## Options Analysis

### Option 1: Arena Allocators (Recommended)

**Pattern**: Pre-allocate large memory region, sub-allocate from it, free all at once.

```c
// Per-request arena
typedef struct {
    char *base;
    size_t size;
    size_t offset;
} arena_t;

void *arena_alloc(arena_t *a, size_t bytes) {
    if (a->offset + bytes > a->size) return NULL;
    void *ptr = a->base + a->offset;
    a->offset += bytes;
    return ptr;
}

void arena_reset(arena_t *a) { a->offset = 0; }
```

**Usage**:
```c
// Each IPC message handler gets fresh arena
arena_t arena = {.base = thread_local_buf, .size = 64*1024};

char *msg = arena_alloc(&arena, msg_len);
cJSON *json = cJSON_ParseWithArena(msg, &arena);  // If cJSON supported arenas

// At end of handler:
arena_reset(&arena);  // No per-allocation free!
```

**Pros**:
- Fast (no fragmentation, no malloc overhead)
- Cache-friendly (contiguous memory)
- No memory leaks (reset wipes everything)
- Pattern used by game engines, compilers, high-perf servers

**Cons**:
- Need to estimate max request size
- cJSON doesn't natively support arenas (would need wrapper or fork)

---

### Option 2: Pool Allocators

**Pattern**: Pre-allocate fixed-size blocks, reuse them.

```c
typedef struct {
    char blocks[64][4096];  // 64 blocks of 4KB each
    uint64_t used_bitmap;
} pool_t;

void *pool_alloc(pool_t *p);
void pool_free(pool_t *p, void *ptr);
```

**Best for**: Objects of known, fixed size (connections, context structs).

Already partially used in `http_server.c` connection pool.

---

### Option 3: Length-Prefixed Buffers (Grow-on-demand)

**Pattern**: Track capacity, realloc only when needed.

```c
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buffer_t;

int buffer_append(buffer_t *b, const char *src, size_t n) {
    if (b->len + n > b->cap) {
        size_t new_cap = (b->cap == 0) ? 256 : b->cap * 2;
        while (new_cap < b->len + n) new_cap *= 2;
        
        char *new_data = realloc(b->data, new_cap);
        if (!new_data) return -ENOMEM;
        
        b->data = new_data;
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 0;
}
```

**Pros**: Handles arbitrarily large data  
**Cons**: Still malloc/realloc (but less often)

---

### Option 4: Two-Pass Parsing

**Pattern**: First pass counts size, second pass copies.

```c
// Phase 1: Calculate needed size
size_t needed = calculate_response_size(data);

// Phase 2: Allocate exact size
char *buf = arena_alloc(&arena, needed);

// Phase 3: Serialize
serialize_response(buf, needed, data);
```

**Best for**: Response serialization where you know structure upfront.

---

## Recommendation

**Hybrid approach**:

| Use Case | Strategy |
|----------|----------|
| IPC message handling | Arena per request (64KB default) |
| Connection structs | Pool allocator (already exists) |
| Response building | Length-prefixed buffer (reuse across requests) |
| Large JSON | Streaming parser (if we switch from cJSON) |

### Implementation Order

1. **Define `arena_t`** in `include/memory.h`
2. **Add thread-local arena** per HTTP/IPC worker
3. **Wrap cJSON** to use arena for scratch space (even if cJSON mallocs internally for result)
4. **Add hard limits** with clear error messages:
   ```c
   if (msg_len > MAX_IPC_MESSAGE) {
       log_error("[IPC] Message too large: %zu > %zu", msg_len, MAX_IPC_MESSAGE);
       return -E2BIG;
   }
   ```

---

## Defined Limits

Document these in spec and enforce at runtime:

| Limit | Value | Rationale |
|-------|-------|-----------|
| `MAX_IPC_MESSAGE` | 64 KB | Typical JSON payload |
| `MAX_HTTP_REQUEST` | 1 MB | Large POST bodies |
| `MAX_PLUGIN_ID` | 128 chars | Reverse domain + name |
| `MAX_SEMANTIC_TYPE` | 64 chars | e.g., `schedule.heat_pump.power` |
| `MAX_SCHEDULE_HORIZON` | 168 periods | 1 week at hourly |
