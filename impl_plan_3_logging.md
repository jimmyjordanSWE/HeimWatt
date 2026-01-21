# Implementation Plan 3: Logging Enhancements

> **Priority**: 3  
> **Effort**: Medium  
> **Risk**: Low (additive, non-breaking)

---

## Overview

Enhance the logging system with:
1. **Ring buffer** for WebUI access to recent logs
2. **Structured JSON output** for machine-parseability
3. **Log categories** as first-class fields

This creates an observable system where the WebUI can display live logs.

---

## Current State

```c
// Modules call log_info(), log_error(), etc.
log_info("[PLUGIN] Started: %s", id);

// Output goes to:
// - stdout (with colors)
// - heimwatt.log file
```

---

## Target State

```c
// Enhanced logging with categories and structured data
log_event(LOG_INFO, "plugin", "started", "{\"id\":\"%s\"}", id);

// Output goes to:
// - stdout (human-readable)
// - heimwatt.log (human-readable)
// - Ring buffer (JSON, for WebUI)
// - /api/logs endpoint serves recent entries
```

---

## Tasks

### 3.1 Create Log Ring Buffer

| File | Purpose |
|------|---------|
| `src/core/log_ring.c` | Ring buffer implementation |
| `include/log_ring.h` | Public API |

```c
// log_ring.h
typedef struct log_entry {
    int64_t timestamp;
    int level;
    char category[32];
    char event[64];
    char message[256];
} log_entry;

void log_ring_init(size_t capacity);
void log_ring_push(const log_entry *entry);
size_t log_ring_get_recent(log_entry *out, size_t max_count);
const char *log_ring_to_json(size_t max_count);  // Returns malloc'd string
```

### 3.2 Add Log Callback for Ring Buffer

Integrate with `rxi/log` using its callback mechanism:

```c
// In server.c or logging init
static void log_ring_callback(log_Event *ev) {
    log_entry entry = {
        .timestamp = time(NULL),
        .level = ev->level,
    };
    // Parse category from message prefix [CATEGORY]
    // ...
    log_ring_push(&entry);
}

// During init:
log_add_callback(log_ring_callback, NULL, LOG_TRACE);
```

### 3.3 Create Structured Logging API

```c
// log_structured.h
#define log_event(level, category, event, fmt, ...) \
    log_event_impl(level, category, event, fmt, ##__VA_ARGS__)

void log_event_impl(int level, const char *category, 
                    const char *event, const char *fmt, ...);
```

Usage:
```c
log_event(LOG_INFO, "plugin", "started", "id=%s", id);
log_event(LOG_WARN, "ipc", "disconnect", "plugin=%s reason=%s", id, reason);
log_event(LOG_ERROR, "db", "insert_failed", "type=%d errno=%d", type, err);
```

### 3.4 Add /api/logs Endpoint

In `server.c` HTTP handler:

```c
if (strcmp(req->path, "/api/logs") == 0) {
    char *json = log_ring_to_json(100);  // Last 100 entries
    http_response_set_json(resp, json);
    free(json);
    return;
}
```

### 3.5 Migrate Existing Log Calls (Optional)

Gradually replace:
```c
// Old
log_info("[PLUGIN] Started: %s", id);

// New
log_event(LOG_INFO, "plugin", "started", "id=%s", id);
```

This can be done incrementally. The old calls still work.

---

## Proposed File Structure

```
src/core/
├── log_ring.c         # NEW: Ring buffer for recent logs
├── log_structured.c   # NEW: Structured logging wrapper
└── ...

include/
├── log_ring.h         # NEW: Ring buffer API
├── log_structured.h   # NEW: log_event() macro
└── ...
```

---

## API Design

### Ring Buffer Query

```http
GET /api/logs?count=50&level=warn
```

Response:
```json
{
  "logs": [
    {
      "timestamp": 1737451200,
      "level": "info",
      "category": "plugin",
      "event": "started",
      "message": "id=se.smhi.weather"
    },
    {
      "timestamp": 1737451201,
      "level": "warn",
      "category": "ipc",
      "event": "disconnect",
      "message": "plugin=se.smhi.weather reason=timeout"
    }
  ]
}
```

---

## Verification

```bash
# Build
make clean && make

# Check ring buffer works
./build/bin/heimwatt &
sleep 2
curl http://localhost:8080/api/logs | jq .

# Should show recent log entries as JSON
```

---

## Completion Criteria

- [ ] Ring buffer stores last N log entries
- [ ] `/api/logs` endpoint returns JSON
- [ ] `log_event()` macro works for structured logging
- [ ] Existing `log_*()` calls continue to work
- [ ] No performance regression (ring buffer is O(1))

---

## Dependencies

- Requires: [Plan 1: Cleanup](impl_plan_1_cleanup.md) (recommended)
- Independent of: [Plan 2: IPC Dispatch](impl_plan_2_ipc_dispatch.md)

---

## Next Plan

After completing this, proceed to [Implementation Plan 4: SDK Event Loop](impl_plan_4_sdk_eventloop.md).
