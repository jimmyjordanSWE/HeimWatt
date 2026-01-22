# ss_log — A C99 Logging Library

> **Version**: 1.0.0  
> **License**: MIT  
> **Files**: `ss_log.c`, `ss_log.h`

## Features

- 6 log levels (TRACE → FATAL)
- Free-form category tags for filtering
- File output with rotation
- Console output (optional)
- Thread-safe by default (toggleable)

## Quick Start

```c
#include "ss_log.h"

int main(void) {
    ss_log_config cfg = { .file_path = "app.log" };
    ss_log_init(&cfg);
    
    ss_log_info("HTTP", "Request from %s", ip);   // Macro, not a function
    ss_log_error("DB", "Query failed: %s", err);  // Auto-captures __FILE__, __LINE__
    
    ss_log_fini();
}
```

**Full configuration options:**
```c
ss_log_config cfg = {
    .file_path       = "app.log",    // Required
    .file_max_bytes  = 10*1024*1024, // Rotate at 10MB (0 = no rotation)
    .console_enabled = true,         // Also write to stderr
    .console_color   = true,         // ANSI colors on console
};
```

> Log files are created with timestamps: `app.log.20260121_143205`. On rotation, the current file is closed and a new timestamped file is started. Old files are never deleted.

## Log Levels

| Level | Macro | Purpose |
|-------|-------|---------|
| 0 | `ss_log_trace(cat, fmt, ...)` | Granular debugging: function entry/exit, variable values |
| 1 | `ss_log_debug(cat, fmt, ...)` | Development info: state changes, decision points |
| 2 | `ss_log_info(cat, fmt, ...)` | Normal operations: startup, shutdown, milestones |
| 3 | `ss_log_warn(cat, fmt, ...)` | Potential issues: retry needed, deprecated usage |
| 4 | `ss_log_error(cat, fmt, ...)` | Failures: operation failed but app continues |
| 5 | `ss_log_fatal(cat, fmt, ...)` | Critical: app cannot continue |

- **cat**: Category string (e.g., `"HTTP"`, `"DB"`)
- **fmt**: printf-style format string (`%s`, `%d`, `%f`, etc.)

## Categories

All log macros require a category as the first argument:

```c
ss_log_info("HTTP", "Request received");
ss_log_warn("CACHE", "Cache miss for key %s", key);
```

**Output**:
```
2026-01-21 14:32:05 INFO  [HTTP] server.c:42: Request received
2026-01-21 14:32:06 WARN  [CACHE] cache.c:78: Cache miss for key user_123
```

Categories are free-form strings. Define constants for compile-time checks:
```c
#define CAT_HTTP  "HTTP"
#define CAT_DB    "DB"
```


## Thread Safety

This library is **thread-safe by default** using an internal mutex. You don't need to do anything for multi-threaded programs.

**What is thread safety and why does it matter?**

Logging involves multiple steps: format the message, write to file, write to console. If two threads run these steps at the same time without protection, their outputs get mixed together:

```
2026-01-21 14:32:05 INFO  [HTTP] server.c:42: Req2026-01-21 14:32:05 ERROR [DB] db.c:87: Connection lost
uest received   <-- corrupted!
```

The library prevents this by locking each log call so only one thread logs at a time.

---

**Lock control API:**

| Function | Effect |
|----------|--------|
| `ss_log_lock_disable()` | Turn off locking |
| `ss_log_lock_enable()` | Turn on locking (uses custom lock if set, else internal mutex) |
| `ss_log_set_lock(fn, udata)` | Set custom lock function |
| `ss_log_set_lock(NULL, NULL)` | Clear custom lock, use internal mutex |


