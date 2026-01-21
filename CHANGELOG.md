# HeimWatt Changelog

All notable changes to this project will be documented in this file.

---

## 2026-01-21

### Features
- **10:55**: Implemented SDK Plugin Taxonomy ([impl_plan_taxonomy.md](impl_plan_taxonomy.md)):
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
