# Implementation Plan 2: IPC Command Dispatch Refactoring

> **Priority**: 2  
> **Effort**: Medium  
> **Risk**: Low (internal refactoring, no API changes)

---

## Overview

Refactor the 349-line `handle_json` god function into a clean dispatch table pattern with separate handler functions for each IPC command. This improves testability and maintainability.

---

## Current State

```c
// server.c - handle_json is a 349-line switch/if-else chain
static void handle_json(heimwatt_ctx *ctx, ipc_conn *conn, cJSON *json) {
    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
    if (strcmp(cmd->valuestring, "report") == 0) {
        // 30+ lines of report handling
    } else if (strcmp(cmd->valuestring, "config") == 0) {
        // 20+ lines of config handling
    }
    // ... 11 more command types
}
```

---

## Target State

```c
// ipc_handlers.c (new file)
static void cmd_report_handler(heimwatt_ctx *ctx, ipc_conn *conn, cJSON *json);
static void cmd_config_handler(heimwatt_ctx *ctx, ipc_conn *conn, cJSON *json);
// ... one function per command

// Dispatch table
static const struct {
    const char *name;
    void (*fn)(heimwatt_ctx*, ipc_conn*, cJSON*);
} ipc_handlers[] = {
    {"report",            cmd_report_handler},
    {"config",            cmd_config_handler},
    {"lookup",            cmd_lookup_handler},
    {"check_data",        cmd_check_data_handler},
    {"query_range",       cmd_query_range_handler},
    {"query_latest",      cmd_query_latest_handler},
    {"register_endpoint", cmd_register_endpoint_handler},
    {"http_response",     cmd_http_response_handler},
    {"request_data",      cmd_request_data_handler},
    {"log",               cmd_log_handler},
    {"hello",             cmd_hello_handler},
};

void handle_ipc_command(heimwatt_ctx *ctx, ipc_conn *conn, cJSON *json) {
    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
    if (!cmd || !cJSON_IsString(cmd)) return;
    
    for (size_t i = 0; i < ARRAY_LEN(ipc_handlers); i++) {
        if (strcmp(cmd->valuestring, ipc_handlers[i].name) == 0) {
            ipc_handlers[i].fn(ctx, conn, json);
            return;
        }
    }
    log_warn("[IPC] Unknown command: '%s'", cmd->valuestring);
}
```

---

## Tasks

### 2.1 Create New Files

| File | Purpose |
|------|---------|
| `src/core/ipc_handlers.c` | Implementation of all command handlers |
| `include/ipc_handlers.h` | Internal header (just declares `handle_ipc_command`) |

### 2.2 Extract Each Command Handler

For each of the 11 commands, create a static function:

| Command | Handler Function | Approx LOC |
|---------|-----------------|------------|
| `hello` | `cmd_hello_handler` | 15 |
| `report` | `cmd_report_handler` | 35 |
| `config` | `cmd_config_handler` | 25 |
| `lookup` | `cmd_lookup_handler` | 20 |
| `check_data` | `cmd_check_data_handler` | 20 |
| `query_range` | `cmd_query_range_handler` | 40 |
| `query_latest` | `cmd_query_latest_handler` | 25 |
| `register_endpoint` | `cmd_register_endpoint_handler` | 30 |
| `http_response` | `cmd_http_response_handler` | 25 |
| `request_data` | `cmd_request_data_handler` | 50 |
| `log` | `cmd_log_handler` | 15 |

### 2.3 Update server.c

1. Remove `handle_json` function body
2. Replace with call to `handle_ipc_command`
3. Add `#include "ipc_handlers.h"`

### 2.4 Update Makefile

Add `src/core/ipc_handlers.c` to the build.

---

## Proposed File Structure

```
src/core/
├── ipc.c              # IPC transport (unchanged)
├── ipc_handlers.c     # NEW: Command handlers
├── plugin_mgr.c       # Plugin management (unchanged)
└── ...

include/
├── ipc.h              # IPC transport API (unchanged)
├── ipc_handlers.h     # NEW: Internal, just handle_ipc_command()
└── ...
```

---

## Verification

```bash
# Build
make clean && make

# Run tests
make test

# Manual: Check that all IPC commands still work
# - Plugin registration (hello)
# - Data reporting (report)
# - Config queries (config)
# - etc.
```

---

## Completion Criteria

- [ ] `handle_json` reduced to < 30 LOC (dispatch only)
- [ ] Each command in its own function
- [ ] All IPC commands work correctly
- [ ] No memory leaks (valgrind check)

---

## Dependencies

- Requires: [Plan 1: Cleanup](impl_plan_1_cleanup.md) (optional but recommended)

---

## Next Plan

After completing this, proceed to [Implementation Plan 3: Logging Enhancements](impl_plan_3_logging.md).
