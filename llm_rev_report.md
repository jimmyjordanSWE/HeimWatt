# LLM Code Review Report

## Summary
The codebase generally follows C99 standards with good usage of opaque pointers and separation of concerns in newer modules (`http_server`, `sdk`). However, the core orchestration layer (`server.c`, `plugin_mgr.c`) exhibits signs of "God Object" patterns and coarse-grained locking that may limit scalability.

## Architectural Findings

| Category | Finding | Criticality |
| :--- | :--- | :--- |
| **God Unit** | `heimwatt_run_with_shutdown_flag` in [server.c](src/server.c#L448) acts as a central supervisor for signals, IPC, HTTP, and plugins. | High |
| **God Unit** | `plugin_mgr_check_health` in [plugin_mgr.c](src/core/plugin_mgr.c#L642) mixes process supervision with restart policy logic. | Medium |
| **Coupling** | `heimwatt_ctx` in [server_internal.h](src/server_internal.h#L21) aggregates all system state (DB, IPC, HTTP, Plugins), creating implicit coupling between modules. | Medium |
| **Concurrency** | `db_handle` uses a single mutex ([db.c](src/db/db.c#L30)) for all operations across all backends. This serializes all writes and reads, potentially bottlenecking the system. | High |

## Semantic Analysis

### 1. Resource Management & Safety
- **Positive**: `http_server.c` demonstrates excellent resource management using `conn_ref` / `conn_unref` ([http_server.c](src/net/http_server.c#L488)) to handle async lifecycles safely.
- **Violation**: `load_manifest` in [plugin_mgr.c](src/core/plugin_mgr.c#L110) uses manual cleanup (multiple `fclose`/`free` calls) instead of the project's standard `goto cleanup` pattern, increasing the risk of memory leaks during error paths.
- **Violation**: `db_open` in [db.c](src/db/db.c#L37) manually manages the `db->backends` array resizing/cleanup on failure, which is error-prone.

### 2. Concurrency & Thread Safety
- **Blocking**: `db_insert_tier1` holds the global DB lock while writing to *all* backends sequentially ([db.c](src/db/db.c#L192)). If one backend (e.g., DuckDB or a future network DB) is slow, it blocks the entire data ingestion pipeline.
- **Safety**: `http_server_run` correctly uses atomic flags for the running state, but the interaction between `g_shutdown` and `shutdown_flag` in `server.c` is complex and spread across signal handlers and the main loop.

### 3. API Design
- **Good**: Opaque pointers are consistently used for public APIs (`heimwatt_ctx`, `http_server`, `plugin_mgr`).
- **Inconsistent**: `plugin_mgr_check_health` is a public API but performs internal supervision duties that likely shouldn't be manually called by consumers.

## Recommendations

1. **Refactor Server Loop**: Split `heimwatt_run` into distinct reactors:
   - `signal_reactor`: Handle signals.
   - `ipc_reactor`: Handle plugin IPC.
   - `supervision_loop`: Handle plugin health.
2. **Granular DB Locking**: Move locking to individual backends or use a read-write lock (`pthread_rwlock_t`) for the dispatcher to allow concurrent reads.
3. **Standardize Cleanup**: Refactor `load_manifest` to use the `goto cleanup` idiom.
4. **Decouple Context**: Break `heimwatt_ctx` into smaller context structs passed only to relevant modules (e.g., `ipc_ctx`, `plugin_ctx`).
