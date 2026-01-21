# Implementation Plan 1: Code Cleanup

> **Priority**: 1 (Do First)  
> **Effort**: Low  
> **Risk**: Minimal

---

## Overview

Quick cleanup tasks that remove dead code and fix minor issues. These should be done first because they're low-risk and reduce technical debt before larger refactoring.

---

## Tasks

### 1.1 Remove `db_error_message` Dead Code

| Field | Value |
|-------|-------|
| **Files** | `include/db.h`, `src/db/db.c`, `src/db/csv_backend.c`, `src/db/duckdb_backend.c` |
| **Effort** | 10 minutes |

**Changes**:
1. Remove declaration from `include/db.h`:
   ```c
   // DELETE: const char *db_error_message(const db_handle *db);
   ```

2. Remove any stub implementations in backends

3. Search for callers and remove any dead code paths that check the return value

**Verification**:
```bash
make clean && make
grep -r "db_error_message" src/ include/
```

---

### 1.2 Add Endpoint Cleanup on Plugin Disconnect

| Field | Value |
|-------|-------|
| **Files** | `src/server.c` |
| **Effort** | 15 minutes |

**Changes**:
In `heimwatt_run_with_shutdown_flag`, after `ipc_conn_destroy(&ctx->conns[i])`, add:

```c
// Clean up registered endpoints for this plugin
const char *pid = ipc_conn_plugin_id(ctx->conns[i]);
if (pid) {
    for (int j = 0; j < ctx->registry_count; j++) {
        if (strcmp(ctx->registry[j].plugin_id, pid) == 0) {
            ctx->registry[j] = ctx->registry[--ctx->registry_count];
            j--;  // Recheck swapped entry
        }
    }
}
```

**Verification**:
1. Start server with plugins
2. Kill a plugin process
3. Check `/api/plugins` - endpoints should be removed
4. Restart plugin - endpoints should re-register

---

## Completion Criteria

- [ ] `db_error_message` removed from all files
- [ ] No compiler errors
- [ ] Endpoint cleanup works on plugin disconnect
- [ ] All existing tests pass

---

## Dependencies

None. This plan can be implemented immediately.

---

## Next Plan

After completing this, proceed to [Implementation Plan 2: IPC Command Dispatch](impl_plan_2_ipc_dispatch.md).
