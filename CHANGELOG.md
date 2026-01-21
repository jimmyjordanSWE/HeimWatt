# HeimWatt Changelog

All notable changes to this project will be documented in this file.

---

## 2026-01-21

- **12:12**: Finalized Graceful Shutdown:
  - **Refactoring**: Reverted experimental SignalFD logic to a simpler, dedicated `sigaction` flow for improved reliability in multi-threaded contexts.
  - **Verification**: Confirmed clean exit (Status 0) and proper cleanup logic across core, HTTP, and database layers.
- **11:51**: Fixed Graceful Shutdown and Signal Handling:
  - **Signal Handling**: Replaced `signal()` with `sigaction()` and disabled `SA_RESTART` flag in `src/main.c`. This ensures blocking calls like `epoll_wait` are correctly interrupted by signals.
  - **IPC API**: Updated `ipc_server_poll()` to return `-errno` on failure, allowing the main loop to detect `EINTR`.
  - **Graceful Shutdown**: Confirmed graceful termination of the main loop and HTTP server thread on `SIGINT` (Ctrl+C).
- **11:38**: Resolved Runtime Initialization Errors:
  - **Database**: Fixed DuckDB version incompatibility by removing stale `data/heimwatt.duckdb` and allowing the server to recreate a fresh database.
  - **Network**: Resolved `Bind failed: Address already in use` error for the HTTP server on port 8080.

### Documentation
- **11:28**: Completed [Manual/security.md](Manual/security.md) as full implementation spec (marked NOT IMPLEMENTED). Covers authentication, credential storage, OAuth flow, remote access options, plugin signatures, device safety constraints, and audit trail.

### Features
- **11:07 - 11:16**: IPC Epoll Unification:
  - **IPC**: Added epoll FD to `ipc_server` struct with full lifecycle management.
  - **IPC API**: New functions `ipc_server_poll()`, `ipc_server_get_epoll_fd()`, `ipc_server_unregister_conn()`, `ipc_server_update_conn_events()`, `ipc_server_is_listen_event()`.
  - **Server**: Refactored `heimwatt_run_with_shutdown_flag()` from poll() to epoll_wait() via unified `ipc_server_poll()`.
  - **Architecture**: HTTP and IPC now both use epoll, creating consistent event-driven patterns.
- **10:55 - 11:06**: SDK Event Loop Refactoring:
  - **SDK**: Refactored `sdk_run` to use a dedicated `sdk_eventloop` module in `src/sdk/eventloop.c`.
  - **SDK**: Added `sdk_eventloop` API (create, add_fd, add_ticker) to `include/sdk_eventloop.h`.
  - **SDK**: Extracted specific logic (ipc, backfill) from `lifecycle.c` to improve modularity.
  - **Testing**: Added unit tests for new event loop module.
  
- **10:49 - 10:55**: Verified Logging Enhancements & Fixed IPC Build Regression:
  - Verified logging implementation (Ring Buffer, /api/logs).
  - Fixed build failure in `ipc_handlers.c` (missing `report_task_args` and `cmd_report_task` definitions).

- **10:44**: Implemented Concurrency Strategy (Thread Pool & Signalfd):
  - **Thread Pool**: Added generic, fixed-size thread pool (`src/util/thread_pool.c`) with lightweight task queue.
  - **Signalfd**: Integrated proper non-blocking signal handling for `SIGCHLD` to supervise plugins without `waitpid` polling loops.
  - **Async IPC**: Refactored `server.c` to offload high-volume `CMD_REPORT` messages (sensor data ingestion) to the thread pool, freeing up the main event loop.
  - **Thread-Safe DB**: Added mutex protection to `src/db/db.c` to support concurrent writes from worker threads.
  - **Testing**: Added comprehensive unit tests for thread pool and verified end-to-end integration.

- **10:20**: Implemented SDK Plugin Taxonomy ([impl_plan_taxonomy.md](impl_plan_taxonomy.md)):
  - **Capabilities**: Support for declaring and enforcing plugin capabilities (Actuate, Report, etc.).
  - **Manifest**: Parsing for `capabilities`, `devices`, `credentials`.
  - **SDK API**: Added `sdk_credential_get` and `sdk_device_setpoint`.
  - **IPC**: Handlers for credential access and device control with permission checks.
- **10:00**: Implemented Logging Enhancements ([impl_plan_3_logging.md](impl_plan_3_logging.md)):
  - **Ring Buffer**: Added `log_ring.c/h` for in-memory log storage.
  - **Structured Logging**: Added `log_structured` API.
  - **API**: Added `/api/logs` endpoint to retrieve recent logs.
  - **Integration**: Hooked logging into `server.c`.
  - **Testing**: Added unit tests for ring buffer.

### Refactoring
- **09:40**: Executed [impl_plan_2_ipc_dispatch.md](impl_plan_2_ipc_dispatch.md):
  - Refactored 349-line `handle_json` god function into `ipc_handlers.c` with 11 distinct handler functions.
  - Implemented generic dispatch table pattern for O(N) command lookup (small N=11).
  - Extracted `heimwatt_ctx` to `server_internal.h` for shared access.
  - **Security**: Fixed buffer overflow in `cmd_config_handler` (unsafe `snprintf` usage).

### Cleanup
- **09:35**: Executed [impl_plan_1_cleanup.md](impl_plan_1_cleanup.md):
  - Removed dead code `db_error_message` from DB interface and backends.
  - Implemented endpoint cleanup on plugin disconnect in `server.c`.

### Reviews & Analysis
- **08:30**: LLM semantic review completed. Identified 1 high-severity (handle_json god function at 349 LOC), 2 medium (sdk_run complexity, nested loop depth), 3 minor (free vs mem_free, stale endpoint cleanup, db_error_message stub). Previous review fixes verified.

### Infrastructure
- **08:28**: Fixed Makefile `analyze` target to run all analysis scripts (`structure.py`, `call_chains.py`, `data_flow.py`, `errors.py`, `hotspots.py`, `invariants.py`, `long_functions.py`, `memory_map.py`, `token_count.py`) using project venv.

---

## 2026-01-20

### Reviews & Analysis

- **18:59**: LLM semantic review completed. Identified 3 medium-severity issues: static buffer reuse in plugin metadata accessors, strncpy null-termination risk, SDK IPC buffering gap. 2 minor issues: duplicate log init, duplicate forward decl.

### Fixes
- **19:14**: Fixed all issues from LLM review:
  - `plugin_mgr.c/h`: Refactored `plugin_get_provided_types` and `find_providers_for_type` to use caller-owned buffers (thread-safe, nesting-safe)
  - `http_server.c`: Replaced `strncpy` with `snprintf` for request_id copy
  - `sdk/config.c`: Replaced `strncpy`/`strcpy` with `memcpy` + explicit null termination
  - `sdk/state.c`: Replaced `strncpy` with `memcpy` + explicit null termination  
  - `http_parse.c`: Replaced `sprintf` with bounded `snprintf`
  - `csv_backend.c`: Replaced `atof` with `strtod` for proper error detection
  - `sdk/lifecycle.c`: Now uses buffered `sdk_ipc_recv` for atomic IPC message framing
  - `server.c`: Removed duplicate logging initialization that leaked file handle
  - `http_server.c`: Removed duplicate forward declaration of `conn_alloc`
